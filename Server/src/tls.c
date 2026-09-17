#include "tls.h"
#include "utils.h"

//YEEEEEEEEEEEEEEEEEY MORE STUPID THING TO make browsers shut up

#include <errno.h>
#include <limits.h>
#include <openssl/err.h>
#include <stdio.h>

SSL_CTX *tls_context(void)
{
    char certificate[PATH_MAX], key[PATH_MAX];
    if (get_server_path(certificate, sizeof(certificate), "tls/cert.pem") != OK ||
        get_server_path(key, sizeof(key), "tls/key.pem") != OK) {
        return(NULL);
    }

    SSL_CTX *context = SSL_CTX_new(TLS_server_method());
    if (!context) {
        return(NULL);
    }
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_use_certificate_chain_file(context, certificate) != 1 ||
        SSL_CTX_use_PrivateKey_file(context, key, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(context) != 1) {
        fprintf(stderr, "Could not load TLS certificate/key from $HOME/.server/tls. "
                        "Run ./generate-cert.sh first.\n");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return(NULL);
    }
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
    return(context);
}

int tls_accept(TLSClient *client, SSL_CTX *context, int socket)
{
    client->failed = 1;
    client->ssl = SSL_new(context);
    ERR_clear_error();
    if (!client->ssl || SSL_set_fd(client->ssl, socket) != 1 ||
        SSL_accept(client->ssl) != 1) {
        return(-1);
    }
    client->failed = 0;
    return(0);
}

ssize_t tls_read(TLSClient *client, void *buffer, size_t length)
{
    size_t received = 0;
    ERR_clear_error();
    errno = 0;
    int result = SSL_read_ex(client->ssl, buffer, length, &received);
    if (result == 1) {
        return((ssize_t)received);
    }
    int error = SSL_get_error(client->ssl, result);
    if (error == SSL_ERROR_ZERO_RETURN) {
        return(0);
    }

    // Blocking sockets have a receive timeout. Abandon timed-out or broken TLS
    // connections instead of retrying forever or sending plaintext errors.
    client->failed = 1;
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
    } else if (error != SSL_ERROR_SYSCALL || !errno) {
        errno = EIO;
    }
    return(-1);
}

int tls_write_all(TLSClient *client, const char *buffer, size_t length)
{
    if (client->failed) {
        return(-1);
    }
    while (length > 0) {
        size_t sent = 0;
        ERR_clear_error();
        if (SSL_write_ex(client->ssl, buffer, length, &sent) != 1) {
            client->failed = 1;
            return(-1);
        }
        buffer += sent;
        length -= sent;
    }
    return(0);
}

void tls_close(TLSClient *client)
{
    if (client->ssl) {
        // Send close_notify once; don't wait for the peer's reply.
        if (!client->failed) {
            ERR_clear_error();
            SSL_shutdown(client->ssl);
        }
        SSL_free(client->ssl);
        client->ssl = NULL;
    }
}
