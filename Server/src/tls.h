#ifndef TLS_H
#define TLS_H

#include <openssl/ssl.h>
#include <sys/types.h>

typedef struct {
    SSL *ssl;
    int failed;
    long long user_id;
} TLSClient;

SSL_CTX *tls_context(void);
int tls_accept(TLSClient *client, SSL_CTX *context, int socket);
ssize_t tls_read(TLSClient *client, void *buffer, size_t length);
int tls_write_all(TLSClient *client, const char *buffer, size_t length);
// Frees TLS state; the caller still owns and closes the socket.
void tls_close(TLSClient *client);

#endif
