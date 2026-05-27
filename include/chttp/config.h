#ifndef CHTTP_CONFIG_H
#define CHTTP_CONFIG_H

#include "chttp/logger.h"
#include "clib/arena.h"

typedef struct {
  int port;
  const char *public_dir;
  LogLevel log_level;
  const char *log_filepath;
  int thread_count;
} HttpConfig;

// port < 0 defaults to 8080, *pub_dir of NULL defaults to "."
HttpConfig chttp_config_init(int port, const char *pub_dir);
HttpConfig chttp_config_load(const char *filepath, Arena *arena);

#endif