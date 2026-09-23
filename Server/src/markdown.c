#include "markdown.h"
#include "../vendor/md4c/md4c-html.h"
#include "../vendor/md4c/entity.h"

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
    if (!out->failed && fwrite(text, 1, size, out->stream) != size) {
        out->failed = 1;
    }
}

static int allowed_url(const char *url, size_t length, int image)
{
    if (!length) {
        return(1);
    }
    if ((length >= 8 && strncasecmp(url, "https://", 8) == 0) ||
        (length >= 7 && strncasecmp(url, "http://", 7) == 0) ||
        (!image && length >= 7 && strncasecmp(url, "mailto:", 7) == 0)) {
        return(1);
    }

    /* Relative paths, fragments and queries are allowed. Reject schemes and
     * encoded characters in the first path segment rather than guessing at them. */
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)url[i];
        if (ch == '/' || ch == '#' || ch == '?') {
            return(1);
        }
        if (ch <= ' ' || ch == ':' || ch == '%' || ch == '&' || ch == '\\') {
            return(0);
        }
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

/* Only inspect HTML emitted by MD4C with raw HTML disabled. Code samples
 * contain escaped tags, so they cannot be mistaken for real headings. */
static void slug_character(FILE *stream, unsigned ch)
{
    if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == 160) ch = '-';
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
        fputc((int)ch, stream);
    } else if (ch >= 128 && ch <= 0x10ffff && !(ch >= 0xd800 && ch <= 0xdfff)) {
        if (ch < 0x800) {
            fputc(0xc0 | (ch >> 6), stream);
        } else {
            if (ch < 0x10000) fputc(0xe0 | (ch >> 12), stream);
            else {
                fputc(0xf0 | (ch >> 18), stream);
                fputc(0x80 | ((ch >> 12) & 63), stream);
            }
            fputc(0x80 | ((ch >> 6) & 63), stream);
        }
        fputc(0x80 | (ch & 63), stream);
    }
}

static char *heading_slug(const char *start, const char *end)
{
    char *slug = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&slug, &size);
    if (!stream) return NULL;
    for (const char *p = start; p < end;) {
        if (*p == '<') {
            const char *close = memchr(p, '>', (size_t)(end - p));
            p = close ? close + 1 : end;
        } else if (*p == '&') {
            const char *semi = memchr(p, ';', (size_t)(end - p));
            const ENTITY *entity = semi ? entity_lookup(p, (size_t)(semi - p + 1)) : NULL;
            if (entity) {
                slug_character(stream, entity->codepoints[0]);
                if (entity->codepoints[1]) slug_character(stream, entity->codepoints[1]);
                p = semi + 1;
            } else if (semi && p[1] == '#') {
                char *tail;
                int hex = p[2] == 'x' || p[2] == 'X';
                unsigned long ch = strtoul(p + (hex ? 3 : 2), &tail, hex ? 16 : 10);
                if (tail == semi && ch <= 0x10ffff) slug_character(stream, (unsigned)ch);
                p = semi + 1;
            } else {
                p++; /* Unknown entities are literal text. */
            }
        } else {
            unsigned char ch = (unsigned char)*p++;
            if (ch >= 128) fputc(ch, stream); /* Preserve source UTF-8. */
            else slug_character(stream, ch);
        }
    }
    int failed = ferror(stream);
    if (fclose(stream) != 0) failed = 1;
    if (failed) { free(slug); return NULL; }
    if (!size) { free(slug); return strdup("section"); }
    return slug;
}

static int heading_id_used(const char *id, char **used, size_t count)
{
    /* IDs owned by the todo template must remain unambiguous. */
    static const char *reserved[] = {
        "page-content", "navigation-status", "notes-preview", "preview-heading",
        "edit-notes", "notes-editor", "content", "markdown-help", "save-notes",
        "cancel-notes", "nuke-heading", "nuke-description", "music-player", "music-status"
    };
    for (size_t i = 0; i < sizeof(reserved) / sizeof(*reserved); i++)
        if (strcmp(id, reserved[i]) == 0) return 1;
    for (size_t i = 0; i < count; i++)
        if (strcmp(id, used[i]) == 0) return 1;
    return 0;
}

static char *add_heading_ids(const char *html)
{
    char *result = NULL;
    size_t length = 0, count = 0;
    char **used = NULL;
    FILE *stream = open_memstream(&result, &length);
    if (!stream) return NULL;
    int failed = 0;
    const char *p = html;
    while (*p) {
        if (strncmp(p, "<h", 2) != 0 || p[2] < '1' || p[2] > '6' || p[3] != '>') {
            fputc(*p++, stream);
            continue;
        }
        char closing[] = "</h1>";
        closing[3] = p[2];
        const char *end = strstr(p + 4, closing);
        if (!end) { failed = 1; break; }
        char *slug = heading_slug(p + 4, end);
        if (!slug) { failed = 1; break; }
        char *id = malloc(strlen(slug) + 32);
        char **expanded = realloc(used, (count + 1) * sizeof(*used));
        if (!id || !expanded) {
            free(slug); free(id);
            if (expanded) used = expanded;
            failed = 1; break;
        }
        used = expanded;
        strcpy(id, slug);
        for (size_t suffix = 1; heading_id_used(id, used, count); suffix++)
            snprintf(id, strlen(slug) + 32, "%s-%zu", slug, suffix);
        free(slug);
        used[count++] = id;
        fprintf(stream, "<h%c id=\"%s\">", p[2], id);
        p += 4;
    }
    if (ferror(stream)) failed = 1;
    if (fclose(stream) != 0) failed = 1;
    for (size_t i = 0; i < count; i++) free(used[i]);
    free(used);
    if (failed) { free(result); return NULL; }
    return result;
}

char *render_markdown(const char *source)
{
    if (!source) {
        source = "";
    }
    size_t size = strlen(source);
    if (size > UINT_MAX) {
        return(NULL);
    }

    char *html = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&html, &length);
    if (!stream) {
        return(NULL);
    }
    struct markdown_output output = {.stream = stream};
    unsigned flags = MD_FLAG_NOHTML | MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH |
                     MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEAUTOLINKS;
    int result = md_html(source, (MD_SIZE)size, append_html, &output, flags, 0);
    if (fclose(stream) != 0) {
        output.failed = 1;
    }
    if (result != 0 || output.failed) {
        free(html);
        return(NULL);
    }
    filter_urls(html);
    char *anchored = add_heading_ids(html);
    free(html);
    return anchored;
}
