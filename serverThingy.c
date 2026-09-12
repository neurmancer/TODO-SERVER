#include "src/router.h"
#include "src/handlers.h"
#include "src/database.h"
#include "src/request.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>

#ifndef PORT
#define PORT 8080
#endif
#define BACKLOG 10


int main(void)
{
    int server_sock = -1, client_sock = -1;
    sqlite3 *db = set_db();
    if (!db) return(EXIT_FAILURE);

    struct sockaddr_in server_addr = {0}, client_addr = {0};

    int opt = 1;

    // A disconnected browser must not terminate the server during send().
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal failed");
        goto cleanup;
    }

    if (route("GET",  "/",        send_homepage) < 0 ||
        route("GET",  "/todos/*", send_todo_page) < 0 ||
        route("POST", "/",        handle_post) < 0 ||
        route("POST", "/update",  handle_update) < 0 ||
        route("POST", "/delete",  handle_delete) < 0) {
        goto cleanup;
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket sucked it");
        goto cleanup;
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        goto cleanup;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        goto cleanup;
    }

    if (listen(server_sock, BACKLOG) == -1) {
        perror("listen failed");
        goto cleanup;
    }

    printf("Server is running on http://localhost:%d\n", PORT);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            if (errno == EINTR) continue;
            perror("accept failed");
            goto cleanup;
        }

        // Don't let an idle connection hold this single-threaded server forever.
        struct timeval timeout = {.tv_sec = 5};
        if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
            setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("client timeout setup failed");
            close(client_sock);
            client_sock = -1;
            continue;
        }

        char buf[HTTP_REQUEST_CAPACITY];
        int status = read_http_request(client_sock, buf, sizeof(buf));
        if (status == 200) {
            printf("Request: %.*s\n", (int)strcspn(buf, "\r\n"), buf);
            handle_request(client_sock, db, buf);
        } else if (status != 0) {
            send_request_error(client_sock, status);
        }

        close(client_sock);
        client_sock = -1;
    }

cleanup:
    if (client_sock != -1) close(client_sock);
    if (server_sock != -1) close(server_sock);
    sqlite3_close(db);
    return(EXIT_FAILURE);
}
