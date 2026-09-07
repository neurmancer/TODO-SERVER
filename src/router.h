#ifndef ROUTES_H
    #define ROUTES_H

#include <stddef.h>

typedef void (*Handler)(int client_sock, const char *path, const char *body);
void send_404(int client_sock, const char *path, const char *body);

void route(const char *method, const char *path, Handler handler);
void handle_request(int client_sock, const char *raw_request);

#endif
