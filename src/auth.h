#ifndef AUTH_H
#define AUTH_H
#include "tls.h"
/* Missing credentials preserve local mode; invalid credentials fail startup. */
int auth_init(void);
int auth_enabled(void);
/* Returns one when the request has been handled or rejected. */
int auth_handle(TLSClient *client, const char *method, const char *path,
                const char *raw, const char *body);
#endif
