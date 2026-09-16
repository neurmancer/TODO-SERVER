#include "router.h"
#include "handlers.h"
#include <stdio.h>
#include <string.h>

#define MAX_ROUTES 64

//Fuck...I've become a web-dev...

typedef struct {
    char method[8];
    char path[256];
    Handler handler;
    int is_wildcard;   // A single * matches any part of the path.
} Route;

static Route routes[MAX_ROUTES];
static int route_count = 0;


int route(const char *method, const char *path, Handler handler) {
    if (!method || !path || !handler || method[0] == '\0' || path[0] != '/' ||
        strlen(method) >= sizeof(routes[0].method) ||
        strlen(path) >= sizeof(routes[0].path)) {
        fprintf(stderr, "Invalid route registration\n");
        return(-1);
    }

    const char *wildcard = strchr(path, '*');
    if (wildcard && strchr(wildcard + 1, '*')) {
        fprintf(stderr, "Route may contain only one wildcard: %s\n", path);
        return(-1);
    }

    if (route_count >= MAX_ROUTES) {
        fprintf(stderr, "Too many routes, you fucking maniac\n");
        return(-1);
    }

    Route *r = &routes[route_count++];
    strcpy(r->method, method);
    strcpy(r->path, path);
    r->handler = handler;
    r->is_wildcard = (wildcard != NULL);
    return(0);
}

void handle_request(TLSClient *client, sqlite3 *db, const char *raw) {
    char method[16] = {0};
    char path[512]  = {0};

    if (!raw || sscanf(raw, "%15s %511s", method, path) != 2) {
        send_404(client, db, path, "");
        return;
    }

    // Query parameters aren't part of the route path.
    char *query = strchr(path, '?');
    if (query) *query = '\0';


    const char *body = strstr(raw, "\r\n\r\n");
    if (body) body += 4;
    else body = "";

    for (int i = 0; i < route_count; i++) {
        Route *r = &routes[i];

        if (strcmp(r->method, method) != 0) continue;

        if (r->is_wildcard) {
            const char *wildcard = strchr(r->path, '*');
            size_t prefix_len = (size_t)(wildcard - r->path);
            size_t suffix_len = strlen(wildcard + 1);
            size_t path_len = strlen(path);
            if (path_len >= prefix_len + suffix_len &&
                strncmp(path, r->path, prefix_len) == 0 &&
                strcmp(path + path_len - suffix_len, wildcard + 1) == 0) {
                r->handler(client, db, path, body);
                return;
            }
        } else {
            if (strcmp(path, r->path) == 0) {
                r->handler(client, db, path, body);
                return;
            }
        }
    }

    send_404(client, db, path, body);
}
