#ifndef ROUTES_H
    #define ROUTES_H

#include "tls.h"

#include <sqlite3.h>

typedef void (*Handler)(TLSClient *client, sqlite3 *db, const char *path, const char *body);

// Returns 0 on success, -1 for invalid routes or a full route table.
// A single * matches zero or more characters (including /); first match wins.
int route(const char *method, const char *path, Handler handler);
// The caller owns the database connection and client socket.
void handle_request(TLSClient *client, sqlite3 *db, const char *raw_request);

#endif
