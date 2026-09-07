#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUF 8192

// now what? should I study fucking finite automata?

static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { return NULL; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return(NULL);
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return(NULL);
    }

    size_t read = fread(buf, 1, size, f);
    fclose(f);
    buf[read] = '\0';
    return(buf);
}

static const char *find_var(TemplateVar *vars, size_t count, const char *key, size_t key_len) {
    for (size_t i = 0; i < count; i++) {
        if (strncmp(vars[i].key, key, key_len) == 0 && vars[i].key[key_len] == '\0') {
            return(vars[i].value ? vars[i].value : "");
        }
    }
    return("");
}

char *render_template(const char *filename, TemplateVar *vars, size_t var_count) {
    char *src = read_file(filename);
    if (!src){ return(NULL); }

    size_t capacity = INITIAL_BUF;
    size_t len = 0;
    char *out = malloc(capacity);
    if (!out) {
        free(src);
        return(NULL);
    }

    char *p = src;

    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            char *end = strstr(p + 2, "}}");
            if (!end) {
                break;
            }

            const char *key_start = p + 2;
            size_t key_len = end - key_start;

            // skip whitespace inside {{ key }}
            while (key_len > 0 && (*key_start == ' ' || *key_start == '\t')) {
                key_start++;
                key_len--;
            }

            while (key_len > 0 && (key_start[key_len-1] == ' ' || key_start[key_len-1] == '\t')) {
                key_len--;
            }

            const char *value = find_var(vars, var_count, key_start, key_len);
            size_t value_len = strlen(value);

            if (len + value_len + 1 >= capacity) {
                capacity = (len + value_len + 1) * 2;
                char *tmp = realloc(out, capacity);
                if (!tmp) {
                    free(out);
                    free(src);
                    return(NULL);
                }
                out = tmp;
            }

            memcpy(out + len, value, value_len);
            len += value_len;

            p = end + 2; 
        } 
        
        else {
            if (len + 1 >= capacity) {
                capacity *= 2;
                char *tmp = realloc(out, capacity);
                if (!tmp) {
                    free(out);
                    free(src);
                    return(NULL);
                }
                out = tmp;
            }
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    free(src);
    return(out);
}