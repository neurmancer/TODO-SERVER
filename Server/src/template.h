#ifndef TEMPLATE_H
    #define TEMPLATE_H

#include <stddef.h>
#include <stdio.h>

typedef struct {
    const char *key;
    const char *value;
} TemplateVar;

// Returns a newly allocated string (you must free it). FUUUUCK I GOTTA KEEP THAT IN MIND(yeah I designed myself and I am cussing at myself)
// Returns NULL on error.

char *render_template(const char *filename, TemplateVar *vars, size_t var_count);

// Render into a private temporary file, flushed and positioned at the start.
// byte_count is the sum of fwrite(..., 1, ...) results. Caller must fclose().
// Returns NULL on rendering or file I/O failure.
FILE *render_template_file(const char *filename, TemplateVar *vars,
                           size_t var_count, size_t *byte_count);

#endif
