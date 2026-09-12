#ifndef HANDLERS_H
#define HANDLERS_H
#include <sqlite3.h>

typedef struct {
    int code;
    const char *meaning;
} HttpStatus;

void send_request_error(int client_sock, int code);

void send_404(int client_socket, sqlite3 *db, const char *path, const char *body);
void send_homepage(int client_sock, sqlite3 *db, const char *path, const char *body);
void send_todo_page(int client_sock, sqlite3 *db, const char *path, const char *body);
void handle_post(int client_sock, sqlite3 *db, const char *path, const char *body);
void handle_update(int client_sock, sqlite3 *db, const char *path, const char *body);
void handle_delete(int client_sock, sqlite3 *db, const char *path, const char *body);

#endif
