#ifndef TEMPLATE_H
    #define TEMPLATE_H

#include <stddef.h>

typedef struct {
    const char *key;
    const char *value;
} TemplateVar;

// Returns a newly allocated string (you must free it). FUUUUCK I GOTTA KEEP THAT IN MIND(yeah I designed myself and I am cussing at myself)
// Returns NULL on error.

static char *read_file(const char *filename);

static const char *find_var(TemplateVar *vars, size_t count, const char *key, size_t key_len);
static const char *skip_ws(const char *p);

char *render_template(const char *filename, TemplateVar *vars, size_t var_count);

#endif