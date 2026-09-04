/*


        ============================= YAPPING ===========================

    'Sup? I've returned with yet another network fuckery 
    (local network till I learn how to make this shit world-wide but on the bright side: You can't get hacked if you are not on the internet)
    So I present you the idea of To-Do server so yeah I am making a Fucking CRUD app despite being a low-level gremlin
    What's next? Puttig a tie and attending to meetings?
    
    at least I am not using fucking Flask duh...


    So to sum up today's fuckery: 
        0- Get a local HTTP server running
        1- Make an index.html to serve 
        2- Let yourself create a to-do not for yourself
        3- Generate a subpage of that to-do thingy
        4- Store the html on storage
        5- Let yourself edit that shit (or delete)
        6- And turn this program to a system daemon so you can use this shit 7/24

        Scope creep section:
            1-Get rid of the html storage idea and use a .db to store addresses
            2-Createa a 'template.html' and fill it with the context from database
            3-Suffer while thinking about why I am doing this without jinja-templates
            4-Markdown style rendering (since it's easier to read and shit yk...I got used to it too fast)
            5-



        Eaiser said than done right?

        How hard can it be...


        I need this server since my ideas keep getting bigger and bigger and I need a fucking tracker of those
        What's next coding my own issue-tracker? (tho that would work...AND THIS IS THE REASON THAT I NEED A TO-DO APP)
        And ladies and gentlemen...this is how I am gonna end up with a git clone at 4AM one night

        and I'll be using snake_case to feel like a real C programmer this time

        Started working on this at: 3 September 2026 00.42AM - Planned Due Date:Hopefully before 2027  

        AND FIRST TO-DO IN THE LIST will be 'Make your own web-framework' 
        this shit is getting out of hand


        Well...After roughly ~18 hours of development I've tried the beta version and here are my findings:
            1- site is indeed accessible on http://localhost:8080 when you run it
            2- You can create a todo once (YOLO) 
            3- The second time I've tried it didn't create anything and fucked the site without re-routing
            4- Trying to look into todo subroutes gives you 404 despite file name appears to be matching with the url
            5- I am confused and exhausted...


        Future update 11PM, Same Day:
            Well...I found the bug (it was race condition and my reliance (is that a word?No fucking clue) on recv()) solved it with a 
            mildly les cursed way of getting Content-Length and fixed the cursed delete handler as well as a few minor changes in update handler

        This version -now- relabily? relaiblylyl(yeah I cant't fucking spell) work with html files with local todo folder creation

        As the scope creep suggests, this project will continue on a separate repo for advencements...
        I'll put it's link here when the repo gets created : *Placeholder Lulz*, but before future updates on this one,
        I'll be making my own malloc using sbrk() to distract myself...

        And yeah corpo-like project gets corpo-like attitude 
*/


//Headers that I know why here (at least for my initila draft)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/errno.h>  
#include <err.h>    //Yeah now we're in BSD territory
//Headers that I have no fucking idea about
#include <sys/socket.h>
#include <arpa/inet.h>


#define PORT 8080
#define BACKLOG 10 //This time I know this isn't overkill for bind lol
#define BUF_SIZE 8192   //Two-pages of bytes are good yk...

//These gonna bite me in the ass when I wanna change the infrastructure to .db
void send_404(int client_socket);
void send_homepage(int client_socket);
void send_todo_page(int client_socket, const char *filename);

//Handlers
void handle_update(int client_socket, char *body);
void handle_delete(int client_socket, char *body);
void handle_post(int client_socket, char *body);

//Todo
void create_todo_file(const char *content);


//Helpers
void url_decode(char *str);


int main(void)
{

    int server_sock = 0, client_sock = 0;
    struct sockaddr_in server_addr = { 0 }, client_addr = { 0 }; 

    socklen_t client_len = sizeof(client_addr);
    
    /*
    DESCRIPTION
        sockaddr_in
                Describes an IPv4 Internet domain socket address.

                .sin_port and .sin_addr are stored in network byte order.
                thx man...(pun intended) that helped fucking too much

        and yeah this program will a lot of man page copy-pastes
                Yeah and I checked wtf is 'network-byte-order' apparently it is 
                stored in big-endian unlike most of modern CPU architecture using little-endian...
                What that implies? No fucking clue

    */



    int opt = 1;

    mkdir("todos", 0755);    //I guess that was the 'Fuck it everybody can see it but only I CAN touch it' mode in octal
    //For Future Neuro: This shit will use getenv to get $HOMe then crate .folder (invisible) so I don't get distracted by 218937218 html files
    //But first let serve a fucking html 

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    //AF_INET = Internet as fuck
    //SOCK_STREAM = TCP

    if (server_sock == -1)
    {
        perror("Socket sucked it");
        exit(EXIT_FAILURE); // Macro for 1 just a fancy way to say 1
    }

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    //Poor man's getaddrinfo()
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    //he htons() function converts the unsigned short integer hostshort from host byte order to network byte order.
    //I have no fucking idea what that implies...I think I shouldn't attempt to do this now but I need the to-do app I forget shit
    //Just learned what this does: It converts the host byte-order to network-byte-order for 16bit ints
    //Berkley nerds of the past...WTF?
    //Besides beej had said getaddrinfo() fills those for me...Did I just peel another abstraction layer by accident?

    if ((bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr)) == -1)) {
        perror("Future me use err() with acutal error code for these shits...IT'S GONNA LOOK FANCY in JournalCTL logs(I hope it won't fail tho)");
        exit(EXIT_FAILURE);
    }

    if(listen(server_sock, BACKLOG) == -1)
    {
        perror("Mwah    OwO");
        close(server_sock); //Yk...sockets are file descriptors so we close it as we open them
        exit(1);
    }

        printf("Server is running on http://localhost:%d\n", PORT);
    while (1) {

        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("Faith has spoken");
            continue;
        }

        char buf[BUF_SIZE] = {0};
        ssize_t bytes = recv(client_sock, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) {
            close(client_sock);
            continue;
        }

        buf[bytes] = '\0';

        char method[16] = {0};
        char path[512] = {0};
        sscanf(buf, "%15s %511s", method, path);

        printf("Request: %s %s\n", method, path);

        if (strcmp(method, "POST") == 0) {

            char *header_end = strstr(buf, "\r\n\r\n");
            
            if (!header_end) {
                close(client_sock);
                continue;
            }
            header_end += 4;

            //Content-Length getting attempt vol:1928491
            int content_length = 0;
            char *cl = strstr(buf, "Content-Length:");
            if (cl) {
                sscanf(cl, "Content-Length: %d", &content_length);
            }

            
            int already_have = bytes - (header_end - buf);

            // If don't have the full body yet read EVERYTHING
            if (content_length > 0 && already_have < content_length) {
                int remaining = content_length - already_have;
                if (remaining > 0 && (header_end + already_have + remaining) < (buf + BUF_SIZE - 1)) {
                    ssize_t more = recv(client_sock, header_end + already_have, remaining, 0);
                    if (more > 0) {
                        already_have += more;
                        header_end[already_have] = '\0';
                    }
                }
            }

            char *body = header_end;


            printf("=== FULL BODY (len=%d) ===\n[%s]\n=========================\n", already_have, body);

            if (strcmp(path, "/update") == 0) {
                handle_update(client_sock, body);
            } else if (strcmp(path, "/delete") == 0) {
                handle_delete(client_sock, body);
            } else {
                handle_post(client_sock, body);
            }
        }

        else if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            send_homepage(client_sock);
        }
        
        else if (strncmp(path, "/todos/", 7) == 0) {
            send_todo_page(client_sock, path + 7);
        }
        
        else { send_404(client_sock); }

        close(client_sock);
    }

    close(server_sock);
    return(0);
}


void send_404(int client_socket)
{
        const char *msg =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<h1 style='font-family:sans-serif; color:#ff5555;'>404 -You Lost Lulz</h1>"
        "<a href='/' style='color:#4CAF50;'>Go back home</a>";
        send(client_socket, msg, strlen(msg), 0);
}


void send_homepage(int client_socket)
{
    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n";
    send(client_socket, header, strlen(header), 0);

    const char *top =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>Neuro's Cyberspace™</title>\n"
        "  <style>\n"
        "    body { font-family: sans-serif; background: #111; color: #eee; max-width: 700px; margin: 40px auto; padding: 20px; }\n"
        "    input[type=text] { width: 70%%; padding: 12px; font-size: 16px; border: none; border-radius: 6px; }\n"
        "    button { padding: 12px 20px; font-size: 16px; background: #4CAF50; color: white; border: none; border-radius: 6px; cursor: pointer; }\n"
        "    .card { background: #222; padding: 16px 20px; margin: 12px 0; border-radius: 8px; display: block; text-decoration: none; color: #eee; transition: 0.15s; }\n"
        "    .card:hover { background: #333; transform: translateY(-2px); }\n"
        "    h1 { color: #4CAF50; }\n"
        "    h2 { margin-top: 40px; color: #aaa; font-size: 18px; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>Neuro's Cyberspace™</h1>\n"
        "  <form action=\"/\" method=\"POST\">\n"
        "    <input type=\"text\" name=\"todo\" placeholder=\"What do you need to do?\" required>\n"
        "    <button type=\"submit\">Create</button>\n"
        "  </form>\n"
        "  <h2>Your todos</h2>\n";
    
    send(client_socket, top, strlen(top), 0);

    DIR *dir = opendir("todos");
    //Again will swap with $HOME/.todo 
    
    if (dir) {
        struct dirent *entry;
        int count = 0;


        while ((entry = readdir(dir)) != NULL) {
            //probably this search will be O(n) if I don't organize shit to use binary-search
            if (entry->d_name[0] == '.') { continue; }
            if (strstr(entry->d_name, ".html") == NULL) { continue; }

            char fpath[512] = { 0 };
            snprintf(fpath, sizeof(fpath), "todos/%s", entry->d_name);
            
            char title[512] = { 0 };    //This is an overflow waiting to happen(stack vars are the last thing I trust)
            FILE *file = fopen(fpath, "r"); //Should I use read-binary? No clue
            if (file) {
                fread(title, 1,  sizeof(title)-1, file);
                fclose(file);
            }

            if (title[0] == '\0') {
                strncat(title, entry->d_name, sizeof(title)-1);
            }

            char card[1024] = { 0 }; 
            snprintf(card, sizeof(card), 
            "<a class=\"card\" href=\"/todos/%s\">%s</a>\n",
            entry->d_name, title);

            send(client_socket, card, strlen(card), 0);
            count++;
            //Been gone to get cigs I'll continue when I return
        }
        closedir(dir);

        if (!count) {
            const char *empty = "<p style=\"color:#666;\">No todos yet. Create one above.</p>\n";
            send(client_socket, empty, strlen(empty), 0);
            //WTF can the send 'flags' be
        }
    }

    const char *bottom = "</body>\n</html>\n";  //Bruh hand-rolling HTML is fucking exhausting
    send(client_socket, bottom, strlen(bottom), 0);
}

void send_todo_page(int client_socket, const char *filename)
{
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "todos/%s", filename);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        send_404(client_socket);
        return;
    }

    char filebuf[8192] = {0};
    fread(filebuf, 1, sizeof(filebuf) - 1, f);
    fclose(f);

    // Try to extract clean text
    char content[4096] = {0};

    // Case 1: old format (full HTML with <p style=...>)
    char *start = strstr(filebuf, "<p style=\"font-size: 22px;\">");
    if (start) {
        start += strlen("<p style=\"font-size: 22px;\">");
        char *end = strstr(start, "</p>");
        if (end) {
            size_t len = end - start;
            if (len > sizeof(content) - 1) len = sizeof(content) - 1;
            strncpy(content, start, len);
            content[len] = '\0';
        }
    }

    // Case 2: new format (pure text) or extraction failed
    if (content[0] == '\0') {
        // Just take the whole file, but cut it if it's too long
        strncpy(content, filebuf, sizeof(content) - 1);
    }

    // Send headers
    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n";
    send(client_socket, header, strlen(header), 0);

    // Build clean page
    char page[16384];
    snprintf(page, sizeof(page),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <title>Edit Todo</title>\n"
        "  <style>\n"
        "    body { font-family: sans-serif; background: #111; color: #eee; padding: 40px; max-width: 700px; margin: 0 auto; }\n"
        "    textarea { width: 100%%; height: 150px; padding: 14px; font-size: 16px; border-radius: 8px; border: 1px solid #444; background: #1a1a1a; color: #eee; resize: vertical; }\n"
        "    button { padding: 11px 20px; margin-right: 10px; margin-top: 14px; border: none; border-radius: 6px; cursor: pointer; font-size: 15px; }\n"
        "    .save { background: #4CAF50; color: white; }\n"
        "    .delete { background: #e74c3c; color: white; }\n"
        "    a { color: #4CAF50; text-decoration: none; }\n"
        "    a:hover { text-decoration: underline; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>Edit Todo</h1>\n"
        "  <form action=\"/update\" method=\"POST\">\n"
        "    <input type=\"hidden\" name=\"id\" value=\"%s\">\n"
        "    <textarea name=\"content\">%s</textarea>\n"
        "    <br>\n"
        "    <button type=\"submit\" class=\"save\">Save</button>\n"
        "  </form>\n"
        "  <form action=\"/delete\" method=\"POST\" style=\"display:inline;\">\n"
        "    <input type=\"hidden\" name=\"id\" value=\"%s\">\n"
        "    <button type=\"submit\" class=\"delete\">Delete</button>\n"
        "  </form>\n"
        "  <br><br>\n"
        "  <a href=\"/\">← Back to all todos</a>\n"
        "</body>\n"
        "</html>\n",
        filename, content, filename
    );

    send(client_socket, page, strlen(page), 0);
}


void handle_update(int client_socket, char *body)
{
    //How should I get unique IDs... let's say id=filename.html&content=new+text

    char *id_start = strstr(body, "id=");
    char *content_start = strstr(body, "content=");

    if (!id_start || !content_start) {
        send_homepage(client_socket);
        return;
    }

    id_start += 3;

    char *id_end = strpbrk(id_start, "&\r\n");

    if (id_end) { *id_end = '\0'; }
    url_decode(id_start);

    content_start += 8;
    char *content_end = strpbrk(content_start, "&\r\n");
    if (content_end) { *content_end = '\0'; }
    
    url_decode(content_start);
    
    char fpath[512] = { 0 };
    snprintf(fpath, sizeof(fpath), "todos/%s", id_start);
    
    FILE *file = fopen(fpath, "w");

    if (file) {
        fputs(content_start, file);
        //Btw I never used file fputs or fopen I usually force-close stdout with dup2 to use printf to print shit into files...so this is new to me either
        fclose(file);
        printf("Updated: %s\n",fpath);
    }

    char location[512] = { 0 };

    snprintf(location, sizeof(location), "Location: /todos/%s\r\n", id_start);

    /*
    
        Dev Blog Time!!!!! (Yeah I know you wanna hear me yap another 40 lines so here you go)

        First it's 7PM of the same day...I've been working on this for roughly ~13 hours...
        Yeah I know...pathetic, still don't have a working CRUD app 
        but new ideas occured while I've been working on this so here is a To-do list...in...source code...of a To-Do List...
        I've been wanting to fuck with libcurl for a while now and I found out something called ntfy
        (https://github.com/binwiederhier/ntfy <- here is the repo) and wanna use that to extend my working env
        I'll probably won't get into curl in this file but idea is getting notified on my phone when I create/delete a to-do 
        And knowing myself...probably that's gonna get a way cursed project on the way    
    */

    char redirect[1024];
    snprintf(redirect, sizeof(redirect),
        "HTTP/1.1 303 See Other\r\n"
        "%s"
        "Connection: close\r\n\r\n", location);

    send(client_socket, redirect, strlen(redirect), 0);

}

//I wonder what happens if I put [[gnu::noreturn]] instead of void lol
void handle_post(int client_socket, char *body)
{

    //Handling the POST seems like having a problem
    /*Current suspects(allegedly):
        1-This function itself
        2- url_decode() call in this function
    */

    char *todo_begins = strstr(body, "todo=");
    //Yeah this is a Batman Begins joke
    if (!todo_begins) {
        //When I try to create anything it redirects here which waits hanging...WHYYYYYYYY
        const char *redirect =
                    "HTTP/1.1 303 See Other\r\n"
                    "Location: /\r\n"
                    "Connection: close\r\n\r\n";
        
        send(client_socket, redirect, strlen(redirect), 0);
        return;
    }


    //printf("Unless\n?"); This line never got exectured
    todo_begins += 5;
    char *todo_returns = strpbrk(todo_begins, "&\r\n");

    /*
    The  strpbrk() function locates the first occurrence in the string s of any of the bytes in
        the string accept.
    */

    if (todo_returns) { *todo_returns = '\0'; }
    printf("This ain't working");
    url_decode(todo_begins);

    printf(">>> New shit dropped: %s\n",todo_begins);
    create_todo_file(todo_begins);

    const char *redirect =
        "HTTP/1.1 303 See Other\r\n"
        "Location: /\r\n"
        "Connection: close\r\n\r\n";
    
    send(client_socket, redirect, strlen(redirect), 0);
}

void handle_delete(int client_socket, char *body)
{

    char *id_start = strstr(body, "id=");
    if (!id_start) { 
        const char *redirect =
                "HTTP/1.1 303 See Other\r\n"
                "Location: /\r\n"
                "Connection: close\r\n\r\n";

        send(client_socket, redirect, strlen(redirect), 0);    
        return;/*This null here 'cuz test purposes I guess...my brain gave up*/ 
    }

    id_start += 3;

    char *id_end = strpbrk(id_start, "&\r\n");
    if (id_end) { *id_end = '\0'; }
    url_decode(id_start);

    char filepath[512] = { 0 };
    snprintf(filepath, sizeof(filepath), "todos/%s", id_start);

    if (remove(filepath) == 0) {
        printf("Deleted: %s\n",filepath);
    }
    else {
        perror("You CAN'T GET RID OF ME");
    }

    const char *redirect =
        "HTTP/1.1 303 See Other\r\n"
        "Location: /\r\n"
        "Connection: close\r\n\r\n";
    send(client_socket, redirect, strlen(redirect), 0);
}


void create_todo_file(const char *content)
{

    static int counter = 0;
    time_t now = time(NULL);    //Lol using this without srand() wrapping it feels WRONG

    char filename[256] = { 0 }; //Second idea: use /dev/urandom
    snprintf(filename, sizeof(filename), "todos/%ld_%d.html", now,counter++);
    //I presume counter++ won't cause UB but we'll see 

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Syscall gods have spoken");
        return;
    }

    fprintf(file, "%s", content);
    fclose(file);

    printf("Created: %s\n",filename);
}

void url_decode(char *str)
{
    char *src = str;
    char *dst = str;    //classic <string.h> notation

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        }

        else if(*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16); //Hexadecimal
            src += 3;
        }

        else { *dst++ = *src++; }  
    }
    *dst = '\0'; //U forget that = U fuked
}