#ifndef UTILS_H
    #define UTILS_H

#include <stdlib.h>

#define TITLE_LEN 256
#define CONTENT_LEN 8192

struct todo_data{
    char content[CONTENT_LEN];
    char title[TITLE_LEN];
    unsigned int id;
    unsigned int is_done;
};


enum STATUS {OK, U_FUCKED=-1};  //Yeah I still refuse to put the fucking tie on!

void url_decode(char *str);
enum STATUS get_env(char *buffer, size_t size);

#endif