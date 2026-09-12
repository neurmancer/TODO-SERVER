#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

#define HTTP_MAX_HEADER_SIZE 8192
// The form handlers use 8192-byte, null-terminated body buffers.
#define HTTP_MAX_BODY_SIZE 8191
#define HTTP_REQUEST_CAPACITY (HTTP_MAX_HEADER_SIZE + HTTP_MAX_BODY_SIZE + 1)

// Read one complete request, including the Content-Length bytes of its body.
// Returns 200 on success, 0 for an empty connection, or an HTTP error status.
// The caller sets a socket receive timeout and owns the buffer and socket.
int read_http_request(int client_sock, char *buffer, size_t capacity);

#endif
