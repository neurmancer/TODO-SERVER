#ifndef JUKEBOX_H
#define JUKEBOX_H

#include "tls.h"

void jukebox_init(int listener);
void jukebox_reap(void);
void jukebox_shutdown(void);
// Called only after authentication. Returns 1 when a jukebox route was handled.
int jukebox_handle(TLSClient *client, const char *method, const char *path, const char *raw);

#endif
