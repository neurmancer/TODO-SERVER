#include "request.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

static int header_is(const char *start, size_t length, const char *name)
{
    return(strlen(name) == length && strncasecmp(start, name, length) == 0);
}

static int parse_body_length(char *buffer, size_t header_size, size_t *body_size)
{
    const char *line = strstr(buffer, "\r\n");
    if (!line || line == buffer) return(400);
    line += 2;
    const char *headers_end = buffer + header_size - 2;
    int has_length = 0;
    *body_size = 0;

    while (line < headers_end) {
        const char *end = strstr(line, "\r\n");
        if (!end || end > headers_end) return(400);
        const char *colon = memchr(line, ':', (size_t)(end - line));
        if (!colon || colon == line) return(400);
        for (const char *p = line; p < colon; p++) {
            if (!isalnum((unsigned char)*p) && !strchr("!#$%&'*+-.^_`|~", *p))
                return(400);
        }

        size_t name_size = (size_t)(colon - line);
        const char *value = colon + 1;
        const char *value_end = end;
        while (value < value_end && (*value == ' ' || *value == '\t')) value++;
        while (value_end > value && (value_end[-1] == ' ' || value_end[-1] == '\t'))
            value_end--;

        // Chunked requests are not supported by this small server.
        if (header_is(line, name_size, "Transfer-Encoding")) return(501);
        // Reject Expect explicitly instead of waiting for a body the client
        // won't send until it receives a 100 Continue response.
        if (header_is(line, name_size, "Expect")) return(417);
        if (header_is(line, name_size, "Content-Length")) {
            if (has_length || value == value_end) return(400);
            has_length = 1;
            for (const char *p = value; p < value_end; p++) {
                if (*p < '0' || *p > '9') return(400);
                size_t digit = (size_t)(*p - '0');
                if (*body_size > (HTTP_MAX_BODY_SIZE - digit) / 10) return(413);
                *body_size = *body_size * 10 + digit;
            }
        }
        line = end + 2;
    }

    if (strncmp(buffer, "POST ", 5) == 0 && !has_length) return(411);
    return(200);
}

int read_http_request(int client_sock, char *buffer, size_t capacity)
{
    if (!buffer || capacity < 2) return(413);
    size_t received = 0;
    size_t header_limit = capacity - 1;
    if (header_limit > HTTP_MAX_HEADER_SIZE) header_limit = HTTP_MAX_HEADER_SIZE;
    size_t target_size = header_limit;
    int headers_ready = 0;
    buffer[0] = '\0';

    for (;;) {
        ssize_t count = recv(client_sock, buffer + received, target_size - received, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) return((errno == EAGAIN || errno == EWOULDBLOCK) ? 408 : 400);
        if (count == 0) return(received == 0 ? 0 : 400);
        // Downstream handlers accept C strings, not embedded null bytes.
        if (memchr(buffer + received, '\0', (size_t)count)) return(400);
        received += (size_t)count;
        buffer[received] = '\0';

        if (!headers_ready) {
            char *end = strstr(buffer, "\r\n\r\n");
            if (!end) {
                if (received == header_limit) return(431);
                continue;
            }
            size_t header_size = (size_t)(end + 4 - buffer);
            size_t body_size = 0;
            int status = parse_body_length(buffer, header_size, &body_size);
            if (status != 200) return(status);
            if (body_size >= capacity - header_size) return(413);
            target_size = header_size + body_size;
            headers_ready = 1;
        }
        if (received >= target_size) {
            // One request per connection; ignore any following pipelined data.
            buffer[target_size] = '\0';
            return(200);
        }
    }
}
