#include "utils.h"
#include "handlers.h"
#include "database.h"
#include "template.h"
#include "markdown.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>

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

static const char *get_status_meaning(int code)
{
    for (size_t i = 0; i < sizeof(status_table) / sizeof(status_table[0]); i++) {
        if (status_table[i].code == code) {
            return(status_table[i].meaning);
        }
    }
    return("Unknown Status");
}


static int send_all(TLSClient *client, const char *data, size_t length)
{
    return(tls_write_all(client, data, length));
}

static int send_headers(TLSClient *client, int code, const char *content_type,
                        size_t length, const char *location)
{
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "%s%s%s"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, get_status_meaning(code), content_type, length,
        location ? "Location: " : "", location ? location : "",
        location ? "\r\n" : "");

    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        return(-1);
    }
    return(send_all(client, header, (size_t)header_len));
}

static void send_http_response(TLSClient *client, int code, const char *content_type, const char *body)
{
    size_t length = body ? strlen(body) : 0;
    if (send_headers(client, code, content_type, length, NULL) == 0 && length) {
        send_all(client, body, length);
    }
}

static void send_redirect(TLSClient *client, const char *location)
{
    send_headers(client, 303, "text/plain; charset=utf-8", 0, location);
}

void send_request_error(TLSClient *client, int code)
{
    send_http_response(client, code, "text/plain; charset=utf-8", get_status_meaning(code));
}

static void send_file_response(TLSClient *client, int code, const char *content_type,
                               FILE *file, size_t length)
{
    if (send_headers(client, code, content_type, length, NULL) < 0) {
        return;
    }

    char buffer[16384];
    while (length > 0) {
        size_t count = length < sizeof(buffer) ? length : sizeof(buffer);
        size_t read_count = fread(buffer, 1, count, file);
        if (read_count == 0 || send_all(client, buffer, read_count) < 0) {
            return;
        }
        length -= read_count;
    }
}

static void send_rendered_page(TLSClient *client, const char *filename,
                               TemplateVar *vars, size_t count)
{
    char page_path[PATH_MAX];
    if (get_server_path(page_path, sizeof(page_path), filename) != OK) {
        send_http_response(client, 500, "text/plain", "Could not resolve page path");
        return;
    }
    size_t length = 0;
    FILE *page = render_template_file(page_path, vars, count, &length);
    if (!page) {
        send_http_response(client, 500, "text/plain", "Could not render page");
        return;
    }
    send_file_response(client, 200, "text/html; charset=utf-8", page, length);
    fclose(page);
}

// Database text must stay text when inserted into HTML or a textarea.
static char *escape_html(const char *text)
{
    if (!text) {
        text = "";
    }
    size_t length = strlen(text);
    if (length > (SIZE_MAX - 1) / 6) {
        return(NULL);
    }
    char *escaped = malloc(length * 6 + 1);
    if (!escaped) {
        return(NULL);
    }
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
        } 
        
        else {
            *out++ = *text;
        }
    }
    *out = '\0';
    return(escaped);
}

void send_404(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)db; (void)path; (void)body;
    char page_path[PATH_MAX];
    FILE *page = get_server_path(page_path, sizeof(page_path), "frontend/404.html") == OK
        ? fopen(page_path, "rb") : NULL;
    struct stat info;
    if (!page || fstat(fileno(page), &info) < 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || (uintmax_t)info.st_size > SIZE_MAX) {
        if (page) {
            fclose(page);
        }
        // Keep 404 usable if the custom page is missing or unreadable.
        send_http_response(client, 404, "text/plain", "404 - Not Found");
        return;
    }
    send_file_response(client, 404, "text/html; charset=utf-8", page, (size_t)info.st_size);
    fclose(page);
}

// Walk from the public directory without following symlinks or parent paths.
// Percent-encoded paths are intentionally rejected, rather than decoded as form data.
static FILE *open_asset(const char *path, struct stat *info)
{
    char relative[512];
    if (!path || path[0] != '/' || strlen(path) >= sizeof(relative) ||
        strpbrk(path, "%\\") || path[strlen(path) - 1] == '/') {
        return(NULL);
    }
    strcpy(relative, path + 1);

    char frontend_path[PATH_MAX];
    if (get_server_path(frontend_path, sizeof(frontend_path), "frontend") != OK) {
        return(NULL);
    }
    int fd = open(frontend_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return(NULL);
    }
    char *save = NULL;
    char *part = strtok_r(relative, "/", &save);
    while (part) {
        if (part[0] == '.') {
            close(fd);
            return(NULL);
        }
        char *next = strtok_r(NULL, "/", &save);
        int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK;
        if (next) {
            flags |= O_DIRECTORY;
        }
        int child = openat(fd, part, flags);
        close(fd);
        if (child < 0) {
            return(NULL);
        }
        fd = child;
        part = next;
    }
    if (fstat(fd, info) < 0 || !S_ISREG(info->st_mode) || info->st_size < 0 ||
        (uintmax_t)info->st_size > SIZE_MAX) {
        close(fd);
        return(NULL);
    }
    FILE *file = fdopen(fd, "rb");
    if (!file) {
        close(fd);
    }
    return(file);
}

static void send_asset(TLSClient *client, sqlite3 *db, const char *path,
                       const char *body, const char *extension, const char *content_type)
{
    const char *suffix = path ? strrchr(path, '.') : NULL;
    struct stat info;
    FILE *file = suffix && strcmp(suffix, extension) == 0 ? open_asset(path, &info) : NULL;
    if (!file) {
        send_404(client, db, path, body);
        return;
    }
    send_file_response(client, 200, content_type, file, (size_t)info.st_size);
    fclose(file);
}

void send_css(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    send_asset(client, db, path, body, ".css", "text/css; charset=utf-8");
}

void send_js(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    send_asset(client, db, path, body, ".js", "text/javascript; charset=utf-8");
}

void send_favicon(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    if (path && strcmp(path, "/favicon.png") == 0) {
        send_asset(client, db, path, body, ".png", "image/png");
    } else if (path && strcmp(path, "/favicon.svg") == 0) {
        send_asset(client, db, path, body, ".svg", "image/svg+xml");
    } else if (path && strcmp(path, "/favicon.ico") == 0) {
        send_asset(client, db, path, body, ".ico", "image/vnd.microsoft.icon");
    } else {
        send_404(client, db, path, body);
    }
}

//You thought this wasn't gonna have a jukebox? Nah you're trippin'
//Selecting the songs rigorusly was harder than the whole fucking project lol
//Direct HTTPS audio links; each one must play without signing in.
static const char *const jukebox_songs[] = {
"https://drive.google.com/uc?export=open&id=1vzC0YESu0JxHRP6hxKO1TS54FOXHw3c5", //Bring Me To Life - The OG
"https://drive.google.com/uc?export=open&id=1Ea99sesm8Ho-gp08jaySqgwXB_-MR-oj", //Lithium
"https://drive.google.com/uc?export=open&id=1_bPQGDr58Sk_gZLP_FgB-0O7RMi5f6sE", //My Immortal
"https://drive.google.com/uc?export=open&id=19XGnFxFeJG68XmTbckq5_BpfkIK5udey", //I Hate Everything About You (that's you J*va)
"https://drive.google.com/uc?export=open&id=1YdCiss7e9pFfuuhDxpAB91kjxflPOdw5", //Pain (Javascript)
"https://drive.google.com/uc?export=open&id=1XHR_qNuP2VCISxJrhnzFqrNKhS8XpfeN", //Boulevard of Broken Dreams
"https://drive.google.com/uc?export=open&id=18G2SYGLKntyhzgLXZJ0LJdmkNkxpJf5E", //Unforgiven (CSS)
"https://drive.google.com/uc?export=open&id=1CpFLSZwQliL9uDfYplewixTY4iuvlid4", //Fade To Black
"https://drive.google.com/uc?export=open&id=1fPDH35PpZqSxNUxW0Pxrakv5iSvKi8u1", //How You Remind Me
"https://drive.google.com/uc?export=open&id=1C-IcedC0m2aRJnGNpLztS-960AE4QIP4", //Lithium
"https://drive.google.com/uc?export=open&id=10XGgVvBJJkP6XLux0OhKcqzENPZ07nF0", //Still Loving You (yup...that's her)
"https://drive.google.com/uc?export=open&id=1fJNOgM3PMav_vf0MwhOR7ZVnYDfH-dLh", //Don't Fear The Reaper
"https://drive.google.com/uc?export=open&id=1QmFON5P2dDa0_0-Qwn_l6iPlNsdhyYQc", //Angel With A Shotgun
"https://drive.google.com/uc?export=open&id=1mFC4ZYCDnyHU7kAkYi_ujsKwZGU_imBN", //Rockefeller Street
"https://drive.google.com/uc?export=open&id=1OW6MmzzRFk-fN7ztpcm-Zwm3LnB8KYJO", //Teeth
"https://drive.google.com/uc?export=open&id=1uU2mjKzS-wUUxcwGJ4Dsv2tDOx0zfWeE", //Take a Hint
"https://drive.google.com/uc?export=open&id=1AcytXQetZIAHnfpg7parij8JTmKTSKiT", //How to Be a Heartbreaker
"https://drive.google.com/uc?export=open&id=165F_05Gmr-iz5gjTTAjcwaEB9pZpHWLz", //Light it Up
"https://drive.google.com/uc?export=open&id=1mHtT9xwhyjNZKxpPsza0UgUsQTol0kZf", //Pretty Rave Girl
"https://drive.google.com/uc?export=open&id=1tYChoSq68A3IiyQ9A8oUe03PVOLfTAwd", //All I Ever Wanted
"https://drive.google.com/uc?export=open&id=1sU39rFpMAb4ajAFvGAsGpu3oJCv540FH", //Angel of Darkness
"https://drive.google.com/uc?export=open&id=1AcSaE1naXCEZgP4RJVIx__Kv-Pg5D3Bf", //Pretty Little Psycho
"https://drive.google.com/uc?export=open&id=1S2kGe2IF2jO-F8mGReNb16eULHVrDo4g", //Maps (Rock Version 'cuz why not
"https://drive.google.com/uc?export=open&id=1j6quK4yfQ-V4rpmDECUnWg5xglOeF22d", //Ace of Spades
"https://drive.google.com/uc?export=open&id=1WBJrOQTFzlmkQ5z0Q_EBfvBTMcuXSK3J", //Too Late Too Late
"https://drive.google.com/uc?export=open&id=1hEIX3vW2nalBgn2V0gyzn4iHrDjmDglz", //Am I Evil? (nah...I'm just autistic AF)
"https://drive.google.com/uc?export=open&id=1TA4Kpj3Oc_Cz70RUaMzlviL1uRwTf_IV", //In the Heat of The Night
"https://drive.google.com/uc?export=open&id=1hrL2x7x7SRdEeJdehbuwmSik_dJBcuGR", //Denim & Leather
"https://drive.google.com/uc?export=open&id=1MesOIRuHZqty4OqvFhEvqjJjMuKmu6T6", //Fire in The Sky
"https://drive.google.com/uc?export=open&id=1gKy2Juy8sBNvNUoZhSK--go2LSb1niv-", //The Trooper
"https://drive.google.com/uc?export=open&id=1S6Ikwiyt6q5fQBJiyi8cBPpg5btZXKb2", //Prisoner of Your (Hazel) Eyes
"https://drive.google.com/uc?export=open&id=1p1r9InaIGQYSbouOabUicTRJw2phNTfq", //Toxic
"https://drive.google.com/uc?export=open&id=1vICbwuedVzN-k5nt3wc5hIZECTCZyMRE", //Gimme More
"https://drive.google.com/uc?export=open&id=1AI1qJxkp73IxiYuUrg11U07MkRYoPTQb", //Oops!...I Did It Again
"https://drive.google.com/uc?export=open&id=1L3JPUMiqoykptsdTkF8xzRoOHouyGrbn", //S&M
"https://drive.google.com/uc?export=open&id=1Yx_n1dMvqmZBtweq4yRkReiAy9pl7b5q", //Call Me Maybe
"https://drive.google.com/uc?export=open&id=1XOlk2SbEp8YEQSIm0kLlmyQ59m9WJgfw", //Bad Romance
"https://drive.google.com/uc?export=open&id=1Jo37Lza4Z8caEuWU0mmm1GTdLY1reMye", //Judas
"https://drive.google.com/uc?export=open&id=1rbxz8r20K8FFt5_aDJDp6cFwu9TwuZwP", //If U Seek Amy
"https://drive.google.com/uc?export=open&id=1o0-CpP0r-ATeY_EDvYAaehfi4--4_yii", //I Kissed a Girl
"https://drive.google.com/uc?export=open&id=1VbU4WSPnkc-0szpSDAmLeKa96VebmK2U", //...Baby One More Time
    NULL, /* you forget that you fucked. */
};

void send_jukebox_song(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)db; (void)path; (void)body;
    unsigned int random;
    sqlite3_randomness(sizeof(random), &random);
    size_t count = sizeof(jukebox_songs) / sizeof(jukebox_songs[0]) - 1;
    if (!count) {
        send_http_response(client, 404, "text/plain; charset=utf-8", "No songs configured");
        return;
    }
    send_http_response(client, 200, "text/plain; charset=utf-8",
                       jukebox_songs[random % count]);
}

struct todo_list {
    FILE *stream;
    size_t count;
    int failed;
};

static void append_todo_link(struct todo_data *todo, void *userdata)
{
    struct todo_list *list = userdata;
    if (list->failed) {
        return;
    }
    char *title = escape_html(todo->title);
    if (!title) {
        list->failed = 1;
        return;
    }
    if (fprintf(list->stream,
                "<li class=\"todo-item\"><form class=\"todo-toggle\" action=\"/complete\" method=\"POST\">"
                "<input type=\"hidden\" name=\"id\" value=\"%d\">"
                "<input type=\"hidden\" name=\"completed\" value=\"%d\">"
                "<input type=\"hidden\" name=\"return_to\" value=\"home\">"
                "<button type=\"submit\" class=\"binary-box%s\" data-value=\"%d\" "
                "aria-label=\"%s: %s\" title=\"%s\"></button></form>"
                "<a href=\"/todos/%d\"><span class=\"todo-text\">%s</span></a></li>\n",
                todo->id, todo->is_done ? 0 : 1, todo->is_done ? " is-done" : "",
                todo->is_done ? 1 : 0, todo->is_done ? "Mark as pending" : "Mark as done",
                title, todo->is_done ? "Mark as pending" : "Mark as done",
                todo->id, title) < 0) {
        list->failed = 1;
    }
    free(title);
    list->count++;
}

void send_homepage(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)path; (void)body;

    if (!db) {
        send_http_response(client, 500, "text/plain", "Database is dead");
        return;
    }

    char *links = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&links, &length);
    if (!stream) {
        send_http_response(client, 500, "text/plain", "Could not build todo list");
        return;
    }
    struct todo_list list = {.stream = stream};
    enum STATUS status = foreach_todo(db, append_todo_link, &list);
    if (!list.count && fprintf(stream, "<li>No todos yet.</li>") < 0) {
        list.failed = 1;
    }
    if (fclose(stream) != 0) {
        list.failed = 1;
    }
    if (status != OK || list.failed) {
        free(links);
        send_http_response(client, 500, "text/plain", "Could not build todo list");
        return;
    }
    TemplateVar vars[] = {{"todo_list", links}};
    send_rendered_page(client, "frontend/index.html", vars, 1);
    free(links);
}


void send_todo_page(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)body;

    if (!db) {
        send_http_response(client, 500, "text/plain", "Database is dead-ass");
        return;
    }

    int id = 0;
    if (sscanf(path, "/todos/%d", &id) != 1 || id <= 0) {
        send_404(client, db, path, body);
        return;
    }

    struct todo_data todo = {0};
    if (get_todo(db, id, &todo) != OK) {
        send_404(client, db, path, body);
        return;
    }

    char id_text[32], created_at[32];
    snprintf(id_text, sizeof(id_text), "%d", todo.id);
    snprintf(created_at, sizeof(created_at), "%ld", todo.created_at);
    char *title = escape_html(todo.title);
    char *content = escape_html(todo.content);
    char *content_html = render_markdown(todo.content);
    if (!title || !content || !content_html) {
        send_http_response(client, 500, "text/plain", "Could not render todo");
    } else {
        TemplateVar vars[] = {
            {"id", id_text}, {"title", title}, {"content", content},
            {"content_html", content_html},
            {"done", todo.is_done ? "1" : "0"}, {"created_at", created_at}
        };
        send_rendered_page(client, "frontend/template.html", vars,
                           sizeof(vars) / sizeof(vars[0]));
    }
    free(title);
    free(content);
    free(content_html);

    free(todo.title);
    free(todo.content);
}

void handle_post(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)path;

    if (!db) {
        send_http_response(client, 500, "text/plain", "Database is dead");
        return;
    }

    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body ? body : "", sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *todo_begins = mutable_body;
    while (todo_begins && strncmp(todo_begins, "todo=", 5) != 0) {
        todo_begins = strchr(todo_begins, '&');
        if (todo_begins) {
            todo_begins++;
        }
    }
    if (!todo_begins) {
        send_http_response(client, 400, "text/plain", "Missing todo field");
        return;
    }

    todo_begins += 5;
    char *todo_end = strpbrk(todo_begins, "&\r\n");
    if (todo_end) {
        *todo_end = '\0';
    }

    char decoded[BUF_SIZE];
    strncpy(decoded, todo_begins, sizeof(decoded) - 1);
    decoded[sizeof(decoded) - 1] = '\0';
    url_decode(decoded);

    if (decoded[0] == '\0') {
        send_http_response(client, 400, "text/plain", "Todo must not be empty");
        return;
    }

    printf(">>> New shit dropped into the DB: %s\n", decoded);

    if (add_todo(db, decoded, decoded) != OK) {
        fprintf(stderr, "FUCK: add_todo failed\n");
        send_http_response(client, 500, "text/plain", "Failed to create todo");
        return;
    }

    send_redirect(client, "/");
}


/* Read one form field, rejecting duplicates, truncation and encoded NULs. */
static int form_field(const char *body, const char *name, char *out, size_t capacity)
{
    size_t name_length = strlen(name);
    int found = 0;
    out[0] = '\0';
    for (const char *field = body ? body : ""; *field;) {
        const char *end = strchr(field, '&');
        size_t length = end ? (size_t)(end - field) : strlen(field);
        if (length > name_length && strncmp(field, name, name_length) == 0 && field[name_length] == '=') {
            size_t value_length = length - name_length - 1;
            if (found || value_length >= capacity){ return(-1); }
            memcpy(out, field + name_length + 1, value_length);
            out[value_length] = '\0';
            for (const char *encoded = out; *encoded; encoded++) {
                if (*encoded != '%') {
                    continue;
                }
                if (strspn(encoded + 1, "0123456789abcdefABCDEF") < 2 || (encoded[1] == '0' && encoded[2] == '0')){ return(-1); }
                encoded += 2;
            }

            url_decode(out);
            found = 1;
        }

        if (!end){ break; }
        field = end + 1;
    }

    return(found);
}

void handle_complete(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)path;
    if (!db) {
        send_http_response(client, 500, "text/plain", "Database is dead");
        return;
    }

    char id_text[64], completed[16], return_to[32];
    if (form_field(body, "id", id_text, sizeof(id_text)) != 1 ||
        form_field(body, "completed", completed, sizeof(completed)) != 1 ||
        form_field(body, "return_to", return_to, sizeof(return_to)) < 0 ||
        !id_text[0] || strspn(id_text, "0123456789") != strlen(id_text) ||
        (strcmp(completed, "0") != 0 && strcmp(completed, "1") != 0) ||
        (return_to[0] && strcmp(return_to, "home") != 0 && strcmp(return_to, "detail") != 0)) {
    
        send_http_response(client, 400, "text/plain", "Invalid completion fields");
        return;
    }

    errno = 0;
    long parsed_id = strtol(id_text, NULL, 10);
    if (errno == ERANGE || parsed_id <= 0 || parsed_id > INT_MAX) {
        send_http_response(client, 400, "text/plain", "Invalid id");
        return;
    }

    int id = (int)parsed_id;
    enum STATUS status = set_todo_completed(db, id, completed[0] == '1');
    if (status == TODO_NOT_FOUND) {
        send_404(client, db, path, body);
        return;
    }
    if (status != OK) {
        send_http_response(client, 500, "text/plain", "Completion update failed");
        return;
    }

    char location[64];
    snprintf(location, sizeof(location), "/todos/%d", id);
    send_redirect(client, strcmp(return_to, "home") == 0 ? "/" : location);
}

void handle_update(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)path;

    if (!db) {
        send_http_response(client, 500, "text/plain", "Database is dead");
        return;
    }

    char id_text[64], content[BUF_SIZE];
    if (form_field(body, "id", id_text, sizeof(id_text)) != 1 ||
        form_field(body, "content", content, sizeof(content)) != 1 ||
        !id_text[0] || strspn(id_text, "0123456789") != strlen(id_text)) {
        send_http_response(client, 400, "text/plain", "Invalid id or content");
        return;
    }

    errno = 0;
    long parsed_id = strtol(id_text, NULL, 10);
    if (errno == ERANGE || parsed_id <= 0 || parsed_id > INT_MAX) {
        send_http_response(client, 400, "text/plain", "Invalid id");
        return;
    }
    int id = (int)parsed_id;

    if (update_todo(db, id, content) != OK) {
        fprintf(stderr, "Update failed for id %d\n", id);
        send_http_response(client, 500, "text/plain", "Update failed");
        return;
    }

    printf("Updated todo #%d\n", id);

    char location[64];
    snprintf(location, sizeof(location), "/todos/%d", id);
    send_redirect(client, location);
}


void handle_delete(TLSClient *client, sqlite3 *db, const char *path, const char *body)
{
    (void)path;

    if (!db) {
        send_http_response(client, 500, "text/plain", "Database is dead");
        return;
    }

    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body ? body : "", sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *id_start = strstr(mutable_body, "id=");
    if (!id_start) {
        send_http_response(client, 400, "text/plain", "Missing id");
        return;
    }

    id_start += 3;
    char *id_end = strpbrk(id_start, "&\r\n");
    if (id_end) {
        *id_end = '\0';
    }

    char id_str[64] = {0};
    strncpy(id_str, id_start, sizeof(id_str) - 1);
    url_decode(id_str);

    int id = atoi(id_str);
    if (id <= 0) {
        send_http_response(client, 400, "text/plain", "Invalid id");
        return;
    }

    if (delete_todo(db, id) == OK) {
        printf("Deleted todo #%d – gone forever, motherfucker\n", id);
    }

    else {
        fprintf(stderr, "Could not delete id %d\n", id);
        send_http_response(client, 500, "text/plain", "Delete failed");
        return;
    }

    send_redirect(client, "/");
}
