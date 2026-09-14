#include "markdown.h"
#include "../vendor/md4c/md4c-html.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct markdown_output {
    FILE *stream;
    int failed;
};

static void append_html(const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    struct markdown_output *out = userdata;
    if (!out->failed && fwrite(text, 1, size, out->stream) != size) out->failed = 1;
}

static int allowed_url(const char *url, size_t length, int image)
{
    if (!length) return(1);
    if ((length >= 8 && strncasecmp(url, "https://", 8) == 0) ||
        (length >= 7 && strncasecmp(url, "http://", 7) == 0) ||
        (!image && length >= 7 && strncasecmp(url, "mailto:", 7) == 0)) return(1);

    /* Relative paths, fragments and queries are allowed. Reject schemes and
     * encoded characters in the first path segment rather than guessing at them. */
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)url[i];
        if (ch == '/' || ch == '#' || ch == '?') return(1);
        if (ch <= ' ' || ch == ':' || ch == '%' || ch == '&' || ch == '\\') return(0);
    }
    return(1);
}

static void filter_urls(char *html)
{
    char *read = html;
    char *write = html;
    while (*read) {
        int image = strncmp(read, "<img src=\"", 10) == 0;
        size_t prefix = image ? 10 : strncmp(read, "<a href=\"", 9) == 0 ? 9 : 0;
        if (prefix) {
            char *value = read + prefix;
            char *end = strchr(value, '"');
            if (end && !allowed_url(value, (size_t)(end - value), image)) {
                size_t tag_length = image ? 4 : 2;
                memmove(write, read, tag_length);
                write += tag_length;
                read = end + 1;
                continue;
            }
        }
        *write++ = *read++;
    }
    *write = '\0';
}

char *render_markdown(const char *source)
{
    if (!source) source = "";
    size_t size = strlen(source);
    if (size > UINT_MAX) return(NULL);

    char *html = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&html, &length);
    if (!stream) return(NULL);
    struct markdown_output output = {.stream = stream};
    unsigned flags = MD_FLAG_NOHTML | MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH |
                     MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEAUTOLINKS;
    int result = md_html(source, (MD_SIZE)size, append_html, &output, flags, 0);
    if (fclose(stream) != 0) output.failed = 1;
    if (result != 0 || output.failed) {
        free(html);
        return(NULL);
    }
    filter_urls(html);
    return(html);
}
