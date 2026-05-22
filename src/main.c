#include "chttp/config.h"
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
  printf("\n[INFO] Shutting down server gracefully.\n");
  exit(0);
}

int main(void) {
  Arena config_arena;
  if (arena_init(&config_arena, CONFIG_ARENA_SIZE) != 0) {
    fprintf(stderr,
            "[FATAL] Failed to initialize configuration memory arena.\n");
    return 1;
  }

  HttpConfig config = chttp_config_load("server.toml", &config_arena);

  struct stat path_stat;
  if (stat(config.public_dir, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode)) {
    fprintf(
        stderr,
        "[FATAL] Public directory '%s' does not exist or is inaccessible.\n",
        config.public_dir);
    return 1;
  }

  signal(SIGINT, handle_shutdown);
  signal(SIGTERM, handle_shutdown);
  printf("[INFO] Starting server on port %d...\n", config.port);
  printf("[INFO] Serving static files from: %s\n", config.public_dir);

  if (chttp_listen_and_serve(&config) != 0) {
    fprintf(stderr, "[FATAL] Server runtime failure occurred.\n");
    return 1;
  }

  arena_free(&config_arena);
  return 0;
}