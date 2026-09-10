#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unitypes.h>

void url_decode(char *str)
{
    char *src = str;
    char *dst = str;

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } 
        
        else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } 
        
        else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}


enum STATUS get_cwd(char *buffer,size_t size)
{
    char cwd[2048] = { 0 };
    if(!getcwd(cwd, sizeof(cwd))){
        return(U_FUCKED);
    }
    
    size_t path_len = strlen(cwd);
    if(size < path_len+1)   //Null term space check
    {
        return(U_FUCKED);
    }

    memcpy(buffer, cwd, path_len);

    buffer[path_len] = '\0';
    return(OK);
}


enum STATUS get_env(char *buf, size_t size)
{
    const char *home = getenv("HOME");
    size_t home_path_len = strlen(home);
    if (home == NULL) {
        return(U_FUCKED);
    }

    if (size < home_path_len+1) {
        return(U_FUCKED);
    }
    memcpy(buf, home, home_path_len);
    buf[home_path_len] = '\0';

    return(OK);
}