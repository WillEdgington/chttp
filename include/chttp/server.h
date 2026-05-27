#ifndef CHTTP_SERVER_H
#define CHTTP_SERVER_H

#include "chttp/config.h"
#include "clib/arena.h"
#include <stddef.h>

int chttp_handle_connection(int client_fd, const char *pub_dir,
                            const char *client_ip, Arena *arena);
int chttp_listen_and_serve(HttpConfig *config);

#endif