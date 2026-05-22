#ifndef CHTTP_CONFIG_H
#define CHTTP_CONFIG_H

#include "clib/arena.h"

typedef struct {
  int port;
  const char *public_dir;
} HttpConfig;

// port < 0 defaults to 8080, *pub_dir of NULL defaults to "."
HttpConfig chttp_config_init(int port, const char *pub_dir);
HttpConfig chttp_config_load(const char *filepath, Arena *arena);

#endif