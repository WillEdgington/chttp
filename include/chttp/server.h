#ifndef CHTTP_SERVER_H
#define CHTTP_SERVER_H

#include <stddef.h>

typedef struct {
  int port;
  const char *public_dir;
} HttpConfig;

// port < 0 defaults to 8080, *pub_dir of NULL defaults to "."
HttpConfig chttp_config_init(int port, const char *pub_dir);
void chttp_handle_connection(int client_fd);
int chttp_listen_and_serve(HttpConfig config);

#endif