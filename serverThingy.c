#include "src/router.h"
#include "src/handlers.h"
#include "src/database.h"
#include "src/request.h"

#include <netinet/in.h>
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


/*

    'sup? I am documentation Neuro (after first successful) launch of the server
    I started with 'whatever keeps the shit together' formatting so I future me is (which is the literally me from now) formatting the 
    code before adding CSS and JS routes and daemonize the server 

*/


int main(void)
{
    int server_sock = -1;
    int client_sock = -1;
    sqlite3 *db = set_db();

    if (!db) return(EXIT_FAILURE);

    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};

    int opt = 1;

    // A disconnected browser must not terminate the server during send().
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal failed");
        goto rome;
    }

    if (route("GET",  "/",        send_homepage) < 0 ||
        route("GET",  "/*.css",   send_css) < 0 ||
        route("GET",  "/*.js",    send_js) < 0 ||
        route("GET",  "/todos/*", send_todo_page) < 0 ||
        route("GET",  "/jukebox/song", send_jukebox_song) < 0 ||
        route("POST", "/",        handle_post) < 0 ||
        route("POST", "/complete", handle_complete) < 0 ||
        route("POST", "/update",  handle_update) < 0 || /*Longest if statement I've ever written so far*/
        route("POST", "/delete",  handle_delete) < 0) { goto rome; } 

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket sucked it");
        goto rome;
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        goto rome;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        goto rome;
    }

    if (listen(server_sock, BACKLOG) == -1) {
        perror("listen failed");
        goto rome;
    }

    printf("Server is running on http://localhost:%d\n", PORT);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            if (errno == EINTR) { continue; }

            perror("accept failed");
            goto rome;
        }

        //Idle client fuckery part
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
        } 
        
        else if (status != 0) {
            send_request_error(client_sock, status);
        }

        close(client_sock);
        client_sock = -1;
    }

rome:
    if (client_sock != -1) close(client_sock);
    if (server_sock != -1) close(server_sock);
    sqlite3_close(db);
    return(EXIT_FAILURE);
}
