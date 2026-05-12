#ifndef CHTTP_SERVER_H
#define CHTTP_SERVER_H

#include <stddef.h>

typedef struct {
  int port;
  const char *public_dir;
} chttp_config;

void chttp_handle_connection(int client_fd);
int chttp_listen_and_serve(chttp_config config);

#endif