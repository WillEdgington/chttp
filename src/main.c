#include "chttp/config.h"
#include "chttp/server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// Signal handler to ensure the terminal cleanly disconnects
static void handle_shutdown(int sig) {
  (void)sig;
  printf("\n[INFO] Shutting down server gracefully.\n");
  exit(0);
}

int main(void) {
  HttpConfig config = chttp_config_init(8080, "./public");

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

  return 0;
}