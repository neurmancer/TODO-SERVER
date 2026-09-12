#ifndef ROUTES_H
    #define ROUTES_H

#include <sqlite3.h>

typedef void (*Handler)(int client_sock, sqlite3 *db, const char *path, const char *body);

// Returns 0 on success, -1 for invalid routes or a full route table.
int route(const char *method, const char *path, Handler handler);
// The caller owns the database connection and client socket.
void handle_request(int client_sock, sqlite3 *db, const char *raw_request);

#endif
