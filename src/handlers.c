#include "utils.h"
#include "handlers.h"
#include "database.h"
#include "template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

#ifndef BUF_SIZE
    #define BUF_SIZE 8192
#endif


static const HttpStatus status_table[] = {
    {200, "OK"},
    {303, "See Other"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {408, "Request Timeout"},
    {411, "Length Required"},
    {413, "Content Too Large"},
    {417, "Expectation Failed"},
    {431, "Request Header Fields Too Large"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"}
};

//Initial Prototype is static but may change in the future
static const char *get_status_meaning(int code)
{
    for (size_t i = 0; i < sizeof(status_table) / sizeof(status_table[0]); i++) {
        if (status_table[i].code == code)
            return(status_table[i].meaning);
    }
    return("Unknown Status");
}


static int send_all(int client_sock, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t sent = send(client_sock, data, length, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return(-1);
        data += sent;
        length -= (size_t)sent;
    }
    return(0);
}

static int send_headers(int client_sock, int code, const char *content_type,
                        size_t length, const char *location)
{
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "%s%s%s"
        "Connection: close\r\n"
        "\r\n",
        code, get_status_meaning(code), content_type, length,
        location ? "Location: " : "", location ? location : "",
        location ? "\r\n" : "");

    if (header_len < 0 || (size_t)header_len >= sizeof(header)) return(-1);
    return(send_all(client_sock, header, (size_t)header_len));
}

static void send_http_response(int client_sock, int code, const char *content_type, const char *body)
{
    size_t length = body ? strlen(body) : 0;
    if (send_headers(client_sock, code, content_type, length, NULL) == 0 && length) {
        send_all(client_sock, body, length);
    }
}

static void send_redirect(int client_sock, const char *location)
{
    send_headers(client_sock, 303, "text/plain; charset=utf-8", 0, location);
}

void send_request_error(int client_sock, int code)
{
    send_http_response(client_sock, code, "text/plain; charset=utf-8", get_status_meaning(code));
}

static void send_file_response(int client_sock, int code, FILE *file, size_t length)
{
    if (send_headers(client_sock, code, "text/html; charset=utf-8", length, NULL) < 0)
        return;

    off_t offset = 0;
    while (length > 0) {
        // A transfer can be short; keep sending until the measured file is sent.
        size_t count = length > 1024 * 1024 ? 1024 * 1024 : length;
        ssize_t sent = sendfile(client_sock, fileno(file), &offset, count);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) {
            // Headers are already sent, so don't append a second HTTP response.
            if (sent < 0) perror("sendfile failed");
            return;
        }
        length -= (size_t)sent;
    }
}

static void send_rendered_page(int client_sock, const char *filename,
                               TemplateVar *vars, size_t count)
{
    size_t length = 0;
    FILE *page = render_template_file(filename, vars, count, &length);
    if (!page) {
        send_http_response(client_sock, 500, "text/plain", "Could not render page");
        return;
    }
    send_file_response(client_sock, 200, page, length);
    fclose(page);
}

// Database text must stay text when inserted into HTML or a textarea.
static char *escape_html(const char *text)
{
    if (!text) text = "";
    size_t length = strlen(text);
    if (length > (SIZE_MAX - 1) / 6) return(NULL);
    char *escaped = malloc(length * 6 + 1);
    if (!escaped) return(NULL);
    char *out = escaped;
    for (; *text; text++) {
        const char *entity = NULL;
        switch (*text) {
            case '&': entity = "&amp;"; break;
            case '<': entity = "&lt;"; break;
            case '>': entity = "&gt;"; break;
            case '"': entity = "&quot;"; break;
            case '\'': entity = "&#39;"; break;
        }
        if (entity) {
            size_t size = strlen(entity);
            memcpy(out, entity, size);
            out += size;
        } else {
            *out++ = *text;
        }
    }
    *out = '\0';
    return(escaped);
}

void send_404(int client_sock, sqlite3 *db, const char *path, const char *body)
{
    (void)db; (void)path; (void)body;
    FILE *page = fopen("frontend/404_not_found.html", "rb");
    struct stat info;
    if (!page || fstat(fileno(page), &info) < 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || (uintmax_t)info.st_size > SIZE_MAX) {
        if (page) fclose(page);
        // Keep 404 usable until the custom file has been created.
        send_http_response(client_sock, 404, "text/plain", "404 - Not Found");
        return;
    }
    send_file_response(client_sock, 404, page, (size_t)info.st_size);
    fclose(page);
}

struct todo_list {
    FILE *stream;
    size_t count;
    int failed;
};

static void append_todo_link(struct todo_data *todo, void *userdata)
{
    struct todo_list *list = userdata;
    if (list->failed) return;
    char *title = escape_html(todo->title);
    if (!title) {
        list->failed = 1;
        return;
    }
    if (fprintf(list->stream, "<li><a href=\"/todos/%d\">%s</a></li>\n",
                todo->id, title) < 0) list->failed = 1;
    free(title);
    list->count++;
}

void send_homepage(int client_sock, sqlite3 *db, const char *path, const char *body)
{
    (void)path; (void)body;

    if (!db) {
        send_http_response(client_sock, 500, "text/plain", "Database is dead");
        return;
    }

    char *links = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&links, &length);
    if (!stream) {
        send_http_response(client_sock, 500, "text/plain", "Could not build todo list");
        return;
    }
    struct todo_list list = {.stream = stream};
    enum STATUS status = foreach_todo(db, append_todo_link, &list);
    if (!list.count && fprintf(stream, "<li>No todos yet.</li>") < 0) list.failed = 1;
    if (fclose(stream) != 0) list.failed = 1;
    if (status != OK || list.failed) {
        free(links);
        send_http_response(client_sock, 500, "text/plain", "Could not build todo list");
        return;
    }
    TemplateVar vars[] = {{"todo_list", links}};
    send_rendered_page(client_sock, "frontend/index.html", vars, 1);
    free(links);
}


void send_todo_page(int client_sock, sqlite3 *db, const char *path, const char *body)
{
    (void)body;

    if (!db) {
        send_http_response(client_sock, 500, "text/plain", "Database is dead-ass");
        return;
    }

    int id = 0;
    if (sscanf(path, "/todos/%d", &id) != 1 || id <= 0) {
        send_404(client_sock, db, path, body);
        return;
    }

    struct todo_data todo = {0};
    if (get_todo(db, id, &todo) != OK) {
        send_404(client_sock, db, path, body);
        return;
    }

    char id_text[32], created_at[32];
    snprintf(id_text, sizeof(id_text), "%d", todo.id);
    snprintf(created_at, sizeof(created_at), "%ld", todo.created_at);
    char *title = escape_html(todo.title);
    char *content = escape_html(todo.content);
    if (!title || !content) {
        send_http_response(client_sock, 500, "text/plain", "Could not render todo");
    } else {
        TemplateVar vars[] = {
            {"id", id_text}, {"title", title}, {"content", content},
            {"done", todo.is_done ? "1" : "0"}, {"created_at", created_at}
        };
        send_rendered_page(client_sock, "frontend/template.html", vars,
                           sizeof(vars) / sizeof(vars[0]));
    }
    free(title);
    free(content);

    free(todo.title);
    free(todo.content);
}

void handle_post(int client_sock, sqlite3 *db, const char *path, const char *body)
{
    (void)path;

    if (!db) {
        send_http_response(client_sock, 500, "text/plain", "Database is dead");
        return;
    }

    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body ? body : "", sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *todo_begins = mutable_body;
    while (todo_begins && strncmp(todo_begins, "todo=", 5) != 0) {
        todo_begins = strchr(todo_begins, '&');
        if (todo_begins) todo_begins++;
    }
    if (!todo_begins) {
        send_http_response(client_sock, 400, "text/plain", "Missing todo field");
        return;
    }

    todo_begins += 5;
    char *todo_end = strpbrk(todo_begins, "&\r\n");
    if (todo_end) *todo_end = '\0';

    char decoded[BUF_SIZE];
    strncpy(decoded, todo_begins, sizeof(decoded) - 1);
    decoded[sizeof(decoded) - 1] = '\0';
    url_decode(decoded);

    if (decoded[0] == '\0') {
        send_http_response(client_sock, 400, "text/plain", "Todo must not be empty");
        return;
    }

    printf(">>> New shit dropped into the DB: %s\n", decoded);

    if (add_todo(db, decoded, decoded) != OK) {
        fprintf(stderr, "FUCK: add_todo failed\n");
        send_http_response(client_sock, 500, "text/plain", "Failed to create todo");
        return;
    }

    send_redirect(client_sock, "/");
}


void handle_update(int client_sock, sqlite3 *db, const char *path, const char *body)
{
    (void)path;

    if (!db) {
        send_http_response(client_sock, 500, "text/plain", "Database is dead");
        return;
    }

    char mutable_body[BUF_SIZE];

    strncpy(mutable_body, body ? body : "", sizeof(mutable_body) - 1);

    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *id_start     = strstr(mutable_body, "id=");
    char *content_start = strstr(mutable_body, "content=");

    if (!id_start || !content_start) {
        send_http_response(client_sock, 400, "text/plain", "Missing id or content");
        return;
    }

    id_start += 3;
    char *id_end = strpbrk(id_start, "&\r\n");
    if (id_end) *id_end = '\0';

    content_start += 8;
    char *content_end = strpbrk(content_start, "&\r\n");
    if (content_end) *content_end = '\0';

    char id_str[64] = {0};
    char content[4096] = {0};

    strncpy(id_str, id_start, sizeof(id_str) - 1);
    strncpy(content, content_start, sizeof(content) - 1);

    url_decode(id_str);
    url_decode(content);

    int id = atoi(id_str);

    if (id <= 0) {
        send_http_response(client_sock, 400, "text/plain", "Invalid id");
        return;
    }

    // completed is hardcoded to 0 for now add a checkbox later
    if (update_todo(db, id, content, 0) != OK) {
        fprintf(stderr, "Update failed for id %d\n", id);
        send_http_response(client_sock, 500, "text/plain", "Update failed");
        return;
    }

    printf("Updated todo #%d\n", id);

    char location[64];
    snprintf(location, sizeof(location), "/todos/%d", id);
    send_redirect(client_sock, location);
}


void handle_delete(int client_sock, sqlite3 *db, const char *path, const char *body)
{
    (void)path;

    if (!db) {
        send_http_response(client_sock, 500, "text/plain", "Database is dead");
        return;
    }

    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body ? body : "", sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *id_start = strstr(mutable_body, "id=");
    if (!id_start) {
        send_http_response(client_sock, 400, "text/plain", "Missing id");
        return;
    }

    id_start += 3;
    char *id_end = strpbrk(id_start, "&\r\n");
    if (id_end) *id_end = '\0';

    char id_str[64] = {0};
    strncpy(id_str, id_start, sizeof(id_str) - 1);
    url_decode(id_str);

    int id = atoi(id_str);
    if (id <= 0) {
        send_http_response(client_sock, 400, "text/plain", "Invalid id");
        return;
    }

    if (delete_todo(db, id) == OK) {
        printf("Deleted todo #%d – gone forever, motherfucker\n", id);
    }

    else {
        fprintf(stderr, "Could not delete id %d\n", id);
        send_http_response(client_sock, 500, "text/plain", "Delete failed");
        return;
    }

    send_redirect(client_sock, "/");
}
