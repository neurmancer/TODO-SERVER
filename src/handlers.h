#ifndef HANDLERS_H
#define HANDLERS_H

typedef struct {
    int code;
    const char *meaning;
} HttpStatus;

void create_todo_file(const char *content);
void send_404(int client_sock, const char *path, const char *body);
void send_homepage(int client_sock, const char *path, const char *body);
void send_todo_page(int client_sock, const char *path, const char *body);
void handle_post(int client_sock, const char *path, const char *body);
void handle_update(int client_sock, const char *path, const char *body);
void handle_delete(int client_sock, const char *path, const char *body);

#endif
