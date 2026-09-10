#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INITIAL_BUF 8192

//Now what? Should I study finite automata?

static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return(NULL);

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

/* Helper: skip whitespace */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return(p);
}


char *render_template(const char *filename, TemplateVar *vars, size_t var_count) {
    char *src = read_file(filename);
    if (!src){ return(NULL); };

    size_t capacity = INITIAL_BUF;
    size_t len = 0;
    char *out = malloc(capacity);
    if (!out) {
        free(src);
        return(NULL);
    }

    char *p = src;

    while (*p) {
        /* ========== {{ variable }} ========== */
        if (p[0] == '{' && p[1] == '{') {
            char *end = strstr(p + 2, "}}");
            if (!end) break;

            const char *key_start = skip_ws(p + 2);
            size_t key_len = end - key_start;
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
            continue;
        }

        /* ========== [[ condition ]] / [[ else ]] / [[ endif ]] ========== */
        if (p[0] == '[' && p[1] == '[') {
            char *end = strstr(p + 2, "]]");
            if (!end) break;

            const char *tag_start = skip_ws(p + 2);
            size_t tag_len = end - tag_start;
            while (tag_len > 0 && (tag_start[tag_len-1] == ' ' || tag_start[tag_len-1] == '\t')) {
                tag_len--;
            }
            
            if ((tag_len == 4 && strncmp(tag_start, "else", 4) == 0) ||
                (tag_len == 5 && strncmp(tag_start, "endif", 5) == 0)) {
                // stray else/endif — just skip the FUCKING TAG
                p = end + 2;
                continue;
            }

            /* ---- That's the shit I'm trynna catch [[ varname ]] ---- */
            const char *value = find_var(vars, var_count, tag_start, tag_len);
            int is_true = (strcmp(value, "1") == 0);

            p = end + 2;


            char *else_pos = NULL;
            char *endif_pos = NULL;
            char *scan = p;
            int depth = 1;

            while (*scan && depth > 0) {
                if (scan[0] == '[' && scan[1] == '[') {
                    char *tag_end = strstr(scan + 2, "]]");
                    if (!tag_end){ break; } 

                    const char *inner = skip_ws(scan + 2);
                    size_t inner_len = tag_end - inner;
                    while (inner_len > 0 && (inner[inner_len-1] == ' ' || inner[inner_len-1] == '\t'))
                        inner_len--;

                    if (inner_len == 5 && strncmp(inner, "endif", 5) == 0) {
                        depth--;
                        if (depth == 0) {
                            endif_pos = scan;
                            break;
                        }
                    } 
                    
                    else if (inner_len == 4 && strncmp(inner, "else", 4) == 0 && depth == 1) {
                        else_pos = scan;
                    } 
                    
                    else {
                        depth++;
                    }
                    
                    scan = tag_end + 2;
                } 
                
                else {
                    scan++;
                }
            }

            if (!endif_pos) {
                break;
            }

            char *true_start = p;
            char *true_end   = else_pos ? else_pos : endif_pos;
            char *false_start = else_pos ? (strstr(else_pos, "]]") + 2) : NULL;
            char *false_end   = endif_pos;

            char *block_start = is_true ? true_start : false_start;
            char *block_end   = is_true ? true_end   : false_end;

            if (block_start && block_end && block_start < block_end) {

                size_t block_len = block_end - block_start;
                char *block_copy = malloc(block_len + 1);
                if (!block_copy) {
                    free(out);
                    free(src);
                    return(NULL);
                }
                memcpy(block_copy, block_start, block_len);
                block_copy[block_len] = '\0';

                char *bp = block_copy;
                while (*bp) {
                    if (bp[0] == '{' && bp[1] == '{') {
                        char *bend = strstr(bp + 2, "}}");
                        if (!bend) break;

                        const char *kstart = skip_ws(bp + 2);
                        size_t klen = bend - kstart;
                        while (klen > 0 && (kstart[klen-1] == ' ' || kstart[klen-1] == '\t')) klen--;

                        const char *val = find_var(vars, var_count, kstart, klen);
                        size_t vlen = strlen(val);

                        if (len + vlen + 1 >= capacity) {
                            capacity = (len + vlen + 1) * 2;
                            char *tmp = realloc(out, capacity);
                            if (!tmp) {
                                free(block_copy);
                                free(out);
                                free(src);  //Bruh memory leaks... I gotta run valgrind too 
                                return(NULL);
                            }
                            out = tmp;
                        }
                        memcpy(out + len, val, vlen);
                        len += vlen;
                        bp = bend + 2;
                    } 
                    
                    else {
                    
                        if (len + 1 >= capacity) {
                            capacity *= 2;
                            char *tmp = realloc(out, capacity);
                            if (!tmp) {
                                free(block_copy);
                                free(out);
                                free(src);
                                return(NULL);
                            }
                            out = tmp;
                        }
                        out[len++] = *bp++;
                    }
                }

                free(block_copy);
            }
            p = strstr(endif_pos, "]]") + 2;
            continue;
        }

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

    out[len] = '\0';
    free(src);
    return(out);
}


