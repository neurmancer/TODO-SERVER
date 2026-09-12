#include "src/router.h"
#include "src/handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
//#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define BACKLOG 10
#define BUF_SIZE 8192


//Well... this is where old architecture and new one clashes I am freezing the project at this exact moment because I have no idea to to connect the shit I created with the main...


int main(void)
{
    int server_sock = 0, client_sock = 0;
    struct sockaddr_in server_addr = {0}, client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    int opt = 1;
    mkdir("todos", 0755);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket sucked it");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, BACKLOG) == -1) {
        perror("listen failed");
        close(server_sock);
        exit(1);
    }

    printf("Server is running on http://localhost:%d\n", PORT);

    route("GET",  "/",          send_homepage);
    route("GET",  "/todos/*",   send_todo_page);    //maybe will change
    route("POST", "/",          handle_post);
    route("POST", "/update",    handle_update);
    route("POST", "/delete",    handle_delete);
    //Place holders:
        //route("GET", "/static/style.css", handle_css);
        //route("GET", "/static/i_dk_js.js", handle_js);
        

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("accept failed");
            continue;
        }

        char buf[BUF_SIZE] = {0};
        ssize_t bytes = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) {
            close(client_sock);
            continue;
        }
        buf[bytes] = '\0';

        printf("Request: %.*s\n", (int)strcspn(buf, "\r\n"), buf);

        handle_request(client_sock, buf);

        close(client_sock);
    }

    close(server_sock);
    return 0;
}
