#ifndef UTILS_H
    #define UTILS_H

#include <stdlib.h>

#define TITLE_LEN 256
#define CONTENT_LEN 8192

struct todo_data {
    int id;
    char *title;
    char *content;      // the big yappy boy
    int is_done;
    long created_at;
};


enum STATUS {OK, U_FUCKED=-1};  //Yeah I still refuse to put the fucking tie on!

void url_decode(char *str);
enum STATUS get_env(char *buf, size_t size);
enum STATUS get_cwd(char *buffer, size_t size);


#endif