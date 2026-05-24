#include "chttp/config.h"
#include "chttp/logger.h"
#include "chttp/server.h"
#include "clib/arena.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define CONFIG_ARENA_SIZE 1024 // 1 KB

// Signal handler to ensure the terminal cleanly disconnects
static void handle_shutdown(int sig) {
  (void)sig;
  LOG_INFO("Shutting down server gracefully.");
  chttp_logger_free();
  exit(0);
}

int main(void) {
  if (chttp_logger_init(NULL, LOG_LEVEL_INFO) != 0) {
    fprintf(stderr, "[FATAL] Failed to initialize logger.\n");
    return 1;
  }

  LOG_INFO("Initializing system sub-modules...");

  Arena config_arena;
  if (arena_init(&config_arena, CONFIG_ARENA_SIZE) != 0) {
    fprintf(stderr,
            "[FATAL] Failed to initialize configuration memory arena.\n");
    chttp_logger_free();
    return 1;
  }

  HttpConfig config = chttp_config_load("server.toml", &config_arena);

  struct stat path_stat;
  if (stat(config.public_dir, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode)) {
    LOG_ERROR(
        "[FATAL] Public directory '%s' does not exist or is inaccessible.\n",
        config.public_dir);
    arena_free(&config_arena);
    chttp_logger_free();
    return 1;
  }

  signal(SIGINT, handle_shutdown);
  signal(SIGTERM, handle_shutdown);
  LOG_INFO("Starting server on port %d...", config.port);
  LOG_INFO("Serving static files from: %s", config.public_dir);

  if (chttp_listen_and_serve(&config) != 0) {
    LOG_ERROR("Server runtime failure occurred.");
    arena_free(&config_arena);
    chttp_logger_free();
    return 1;
  }

  arena_free(&config_arena);
  chttp_logger_free();
  return 0;
}