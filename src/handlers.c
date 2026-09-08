#include "utils.h"
#include "handlers.h"


//well...I gotta admit I am quite afraid to migrate that abomination to db without fucking up rn...
//I mean it's broken yeah but at least I can see a fucking webpage 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#ifndef BUF_SIZE
    #define BUF_SIZE 8192
#endif

void send_404(int client_sock, const char *path, const char *body)
{
    (void)path; (void)body;
    const char *msg =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<h1 style='font-family:sans-serif; color:#ff5555;'>404 - You Lost Lulz</h1>"
        "<a href='/' style='color:#4CAF50;'>Go back home</a>";
    send(client_sock, msg, strlen(msg), 0);
}

void send_homepage(int client_sock, const char *path, const char *body)
{
    (void)path; (void)body;

    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n";
    send(client_sock, header, strlen(header), 0);

    const char *top =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>Neuro's Cyberspace™</title>\n"
        "  <style>\n"
        "    body { font-family: sans-serif; background: #111; color: #eee; max-width: 700px; margin: 40px auto; padding: 20px; }\n"
        "    input[type=text] { width: 70%%; padding: 12px; font-size: 16px; border: none; border-radius: 6px; }\n"
        "    button { padding: 12px 20px; font-size: 16px; background: #4CAF50; color: white; border: none; border-radius: 6px; cursor: pointer; }\n"
        "    .card { background: #222; padding: 16px 20px; margin: 12px 0; border-radius: 8px; display: block; text-decoration: none; color: #eee; transition: 0.15s; }\n"
        "    .card:hover { background: #333; transform: translateY(-2px); }\n"
        "    h1 { color: #4CAF50; }\n"
        "    h2 { margin-top: 40px; color: #aaa; font-size: 18px; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>Neuro's Cyberspace™</h1>\n"
        "  <form action=\"/\" method=\"POST\">\n"
        "    <input type=\"text\" name=\"todo\" placeholder=\"What do you need to do?\" required>\n"
        "    <button type=\"submit\">Create</button>\n"
        "  </form>\n"
        "  <h2>Your todos</h2>\n";
    send(client_sock, top, strlen(top), 0);

    DIR *dir = opendir("todos");
    if (dir) {
        struct dirent *entry;
        int count = 0;

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strstr(entry->d_name, ".html") == NULL) continue;

            char fpath[512] = {0};
            snprintf(fpath, sizeof(fpath), "todos/%s", entry->d_name);

            char title[512] = {0};
            FILE *file = fopen(fpath, "r");
            if (file) {
                fread(title, 1, sizeof(title) - 1, file);
                fclose(file);
            }
            if (title[0] == '\0') {
                strncat(title, entry->d_name, sizeof(title) - 1);
            }

            char card[1024] = {0};
            snprintf(card, sizeof(card),
                     "<a class=\"card\" href=\"/todos/%s\">%s</a>\n",
                     entry->d_name, title);
            send(client_sock, card, strlen(card), 0);
            count++;
        }
        closedir(dir);

        if (!count) {
            const char *empty = "<p style=\"color:#666;\">No todos yet. Create one above.</p>\n";
            send(client_sock, empty, strlen(empty), 0);
        }
    }

    const char *bottom = "</body>\n</html>\n";
    send(client_sock, bottom, strlen(bottom), 0);
}

void send_todo_page(int client_sock, const char *path, const char *body)
{
    (void)body;

    // path looks like "/todos/123456_0.html"
    const char *filename = path + 7; // skip "/todos/"

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "todos/%s", filename);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        send_404(client_sock, path, body);
        return;
    }

    char filebuf[8192] = {0};
    fread(filebuf, 1, sizeof(filebuf) - 1, f);
    fclose(f);

    char content[4096] = {0};

    // Try old HTML format first
    char *start = strstr(filebuf, "<p style=\"font-size: 22px;\">");
    if (start) {
        start += strlen("<p style=\"font-size: 22px;\">");
        char *end = strstr(start, "</p>");
        if (end) {
            size_t len = end - start;
            if (len > sizeof(content) - 1) len = sizeof(content) - 1;
            strncpy(content, start, len);
            content[len] = '\0';
        }
    }

    // Fallback to pure text
    if (content[0] == '\0') {
        strncpy(content, filebuf, sizeof(content) - 1);
    }

    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n";
    send(client_sock, header, strlen(header), 0);

    char page[16384];
    snprintf(page, sizeof(page),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <title>Edit Todo</title>\n"
        "  <style>\n"
        "    body { font-family: sans-serif; background: #111; color: #eee; padding: 40px; max-width: 700px; margin: 0 auto; }\n"
        "    textarea { width: 100%%; height: 150px; padding: 14px; font-size: 16px; border-radius: 8px; border: 1px solid #444; background: #1a1a1a; color: #eee; resize: vertical; }\n"
        "    button { padding: 11px 20px; margin-right: 10px; margin-top: 14px; border: none; border-radius: 6px; cursor: pointer; font-size: 15px; }\n"
        "    .save { background: #4CAF50; color: white; }\n"
        "    .delete { background: #e74c3c; color: white; }\n"
        "    a { color: #4CAF50; text-decoration: none; }\n"
        "    a:hover { text-decoration: underline; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>Edit Todo</h1>\n"
        "  <form action=\"/update\" method=\"POST\">\n"
        "    <input type=\"hidden\" name=\"id\" value=\"%s\">\n"
        "    <textarea name=\"content\">%s</textarea>\n"
        "    <br>\n"
        "    <button type=\"submit\" class=\"save\">Save</button>\n"
        "  </form>\n"
        "  <form action=\"/delete\" method=\"POST\" style=\"display:inline;\">\n"
        "    <input type=\"hidden\" name=\"id\" value=\"%s\">\n"
        "    <button type=\"submit\" class=\"delete\">Delete</button>\n"
        "  </form>\n"
        "  <br><br>\n"
        "  <a href=\"/\">← Back to all todos</a>\n"
        "</body>\n"
        "</html>\n",
        filename, content, filename
    );

    send(client_sock, page, strlen(page), 0);
}

void handle_post(int client_sock, const char *path, const char *body)
{
    (void)path;


    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body, sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *todo_begins = strstr(mutable_body, "todo=");
    if (!todo_begins) {
        const char *redirect =
            "HTTP/1.1 303 See Other\r\n"
            "Location: /\r\n"
            "Connection: close\r\n\r\n";
        send(client_sock, redirect, strlen(redirect), 0);
        return;
    }

    todo_begins += 5;
    char *todo_end = strpbrk(todo_begins, "&\r\n");
    if (todo_end) *todo_end = '\0';


    char decoded[BUF_SIZE];
    strncpy(decoded, todo_begins, sizeof(decoded) - 1);
    url_decode(decoded);

    printf(">>> New shit dropped: %s\n", decoded);
    create_todo_file(decoded);

    const char *redirect =
        "HTTP/1.1 303 See Other\r\n"
        "Location: /\r\n"
        "Connection: close\r\n\r\n";
    send(client_sock, redirect, strlen(redirect), 0);
}

void handle_update(int client_sock, const char *path, const char *body)
{
    (void)path;


    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body, sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *id_start = strstr(mutable_body, "id=");
    char *content_start = strstr(mutable_body, "content=");

    if (!id_start || !content_start) {
        send_homepage(client_sock, path, body);
        return;
    }

    id_start += 3;
    char *id_end = strpbrk(id_start, "&\r\n");
    if (id_end) *id_end = '\0';

    content_start += 8;
    char *content_end = strpbrk(content_start, "&\r\n");
    if (content_end) *content_end = '\0';

    char id[256], content[4096];
    strncpy(id, id_start, sizeof(id) - 1);
    strncpy(content, content_start, sizeof(content) - 1);
    url_decode(id);
    url_decode(content);

    char fpath[512] = {0};
    snprintf(fpath, sizeof(fpath), "todos/%s", id);

    FILE *file = fopen(fpath, "w");
    if (file) {
        fputs(content, file);
        fclose(file);
        printf("Updated: %s\n", fpath);
    }

    char redirect[1024];
    snprintf(redirect, sizeof(redirect),
        "HTTP/1.1 303 See Other\r\n"
        "Location: /todos/%s\r\n"
        "Connection: close\r\n\r\n", id);
    send(client_sock, redirect, strlen(redirect), 0);
}

void handle_delete(int client_sock, const char *path, const char *body)
{
    (void)path;


    char mutable_body[BUF_SIZE];
    strncpy(mutable_body, body, sizeof(mutable_body) - 1);
    mutable_body[sizeof(mutable_body) - 1] = '\0';

    char *id_start = strstr(mutable_body, "id=");
    if (!id_start) {
        const char *redirect =
            "HTTP/1.1 303 See Other\r\n"
            "Location: /\r\n"
            "Connection: close\r\n\r\n";
        send(client_sock, redirect, strlen(redirect), 0);
        return;
    }

    id_start += 3;
    char *id_end = strpbrk(id_start, "&\r\n");
    if (id_end) *id_end = '\0';

    char id[256];
    strncpy(id, id_start, sizeof(id) - 1);
    url_decode(id);

    char filepath[512] = {0};
    snprintf(filepath, sizeof(filepath), "todos/%s", id);

    if (remove(filepath) == 0) {
        printf("Deleted: %s\n", filepath);
    } else {
        perror("You CAN'T GET RID OF ME");
    }

    const char *redirect =
        "HTTP/1.1 303 See Other\r\n"
        "Location: /\r\n"
        "Connection: close\r\n\r\n";
    send(client_sock, redirect, strlen(redirect), 0);
}

void create_todo_file(const char *content)
{
    static int counter = 0;
    time_t now = time(NULL);

    char filename[256] = {0};
    snprintf(filename, sizeof(filename), "todos/%ld_%d.html", now, counter++);

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Syscall gods have spoken");
        return;
    }
    fprintf(file, "%s", content);
    fclose(file);
    printf("Created: %s\n", filename);
}