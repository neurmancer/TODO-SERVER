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
#include <ifaddrs.h>
#include <net/if.h>

#ifndef PORT
#define PORT 8080
#endif
#define BACKLOG 10


/*

    'sup? I am documentation Neuro (after first successful) launch of the server
    I started with 'whatever keeps the shit together' formatting so I future me is (which is the literally me from now) formatting the 
    code before adding CSS and JS routes and daemonize the server 

*/

void sigHandler(int sigNum);

volatile sig_atomic_t flag = 1; 

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int server_sock = -1;
    int client_sock = -1;

    TLSClient client = {0};
    SSL_CTX *context = NULL;

    int exit_status = EXIT_FAILURE;

    struct ifaddrs *interfaces = NULL;
    sqlite3 *db = set_db();

    if (!db) { return(EXIT_FAILURE); }

    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};

    struct sigaction sa = {0};

    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    /* Let blocking socket calls return EINTR so shutdown can finish. */
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Signal Fuck Up...");
        goto rome;
    }

    struct sigaction ignore_pipe = {.sa_handler = SIG_IGN};
    sigemptyset(&ignore_pipe.sa_mask);
    if (sigaction(SIGPIPE, &ignore_pipe, NULL) == -1) {
        perror("SIGPIPE setup failed");
        goto rome;
    }
    context = tls_context();
    if (!context) goto rome;

    int opt = 1;

    if (route("GET",  "/",        send_homepage) < 0 ||
        route("GET",  "/*.css",   send_css) < 0 ||
        route("GET",  "/*.js",    send_js) < 0 ||
        route("GET",  "/favicon.svg", send_favicon) < 0 ||
        route("GET",  "/favicon.png", send_favicon) < 0 ||
        route("GET",  "/favicon.ico", send_favicon) < 0 ||
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

    server_addr.sin_family = AF_INET; // Internet As Fuck 
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

    printf("Server is running on https://localhost:%d\n", PORT);
    if (getifaddrs(&interfaces) == -1) {
        perror("getifaddrs failed");
        goto rome;
    }

    for (struct ifaddrs *device = interfaces; device != NULL; device = device->ifa_next) {
        if (device->ifa_addr == NULL || device->ifa_addr->sa_family != AF_INET ||
            !(device->ifa_flags & IFF_UP) || (device->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }

        struct sockaddr_in *address = (struct sockaddr_in *)device->ifa_addr;
        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip)) == NULL) {
            perror("inet_ntop failed");
            goto rome;
        }

        printf("Local network (%s): https://%s:%d\n", device->ifa_name, ip, PORT);
    }
    freeifaddrs(interfaces);
    interfaces = NULL;

    while (flag) {
        
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

        if (tls_accept(&client, context, client_sock) < 0) {
            tls_close(&client);
            close(client_sock);
            client_sock = -1;
            continue;
        }

        char buf[HTTP_REQUEST_CAPACITY];
        int status = read_http_request(&client, buf, sizeof(buf));
        if (!flag) { break; }
        if (status == 200) {
            printf("Request: %.*s\n", (int)strcspn(buf, "\r\n"), buf);
            handle_request(&client, db, buf);
        } 
        
        else if (status != 0) {
            send_request_error(&client, status);
        }

        printf("Server's live\n");


        tls_close(&client);
        close(client_sock);
        client_sock = -1;
    }

    exit_status = EXIT_SUCCESS;

rome:
    tls_close(&client);
    SSL_CTX_free(context);
    if (interfaces != NULL) freeifaddrs(interfaces);
    if (client_sock != -1) close(client_sock);
    if (server_sock != -1) close(server_sock);
    sqlite3_close(db);
    return(exit_status);
}


void sigHandler(int sigNum)
{
    if (sigNum == SIGTERM || sigNum == SIGINT) {
        flag = 0;
    }
}
