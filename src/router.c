#include "router.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ROUTES 64


typedef struct {
    char method[8];
    char path[256];
    Handler handler;
    int is_wildcard;   // 1 if path ends with *
} Route;

static Route routes[MAX_ROUTES];
static int route_count = 0;


void route(const char *method, const char *path, Handler handler) {
    if (route_count >= MAX_ROUTES) {
        fprintf(stderr, "Too many routes, you fucking maniac\n");
        return;
    }

    Route *r = &routes[route_count++];
    strncpy(r->method, method, sizeof(r->method)-1);
    strncpy(r->path, path, sizeof(r->path)-1);
    r->handler = handler;
    r->is_wildcard = (path[strlen(path)-1] == '*');
}

void handle_request(int client_sock, const char *raw) {
    char method[16] = {0};
    char path[512]  = {0};

    sscanf(raw, "%15s %511s", method, path);

    // Find body if it exists
    const char *body = strstr(raw, "\r\n\r\n");
    if (body) body += 4;
    else body = "";

    for (int i = 0; i < route_count; i++) {
        Route *r = &routes[i];

        if (strcmp(r->method, method) != 0) continue;

        if (r->is_wildcard) {
            // super simple: check if path starts with the part before *
            size_t len = strlen(r->path) - 1;
            if (strncmp(path, r->path, len) == 0) {
                r->handler(client_sock, path, body);
                return;
            }
        } else {
            if (strcmp(path, r->path) == 0) {
                r->handler(client_sock, path, body);
                return;
            }
        }
    }

    // no match → 404
    send_404(client_sock, path, body);   // you’ll move this later
}