#ifndef MARKDOWN_H
#define MARKDOWN_H

/* Render a note into an owned HTML string; free it after use. NULL on failure. */
char *render_markdown(const char *source);

#endif
