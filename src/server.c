#include "chttp/server.h"
#include "chttp/handler.h"
#include "chttp/http.h"
#include "clib/arena.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 10
#define BUFFER_SIZE 4096           // 4 KB
#define CONNECTION_ARENA_SIZE 8192 // 8 KB

static int setup_listener(int port) {
  int fd;
  struct sockaddr_in addr;
  int opt = 1;

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    return -1;
  if (listen(fd, BACKLOG) < 0)
    return -1;

  return fd;
}

HttpConfig chttp_config_init(int port, const char *pub_dir) {
  return (HttpConfig){.port = (port <= 0) ? 8080 : port,
                      .public_dir = (pub_dir == NULL) ? "." : pub_dir};
}

int chttp_handle_connection(int client_fd, const char *pub_dir) {
  Arena connection_arena;
  if (arena_init(&connection_arena, CONNECTION_ARENA_SIZE) != 0)
    return -1;

  char *buffer = arena_alloc(&connection_arena, BUFFER_SIZE);
  if (buffer == NULL) {
    arena_free(&connection_arena);
    return -1;
  }

  ssize_t valread = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
  if (valread <= 0) {
    arena_free(&connection_arena);
    return -1;
  }

  HttpRequest *req = chttp_parse_request(buffer, &connection_arena);
  if (req == NULL) {
    arena_free(&connection_arena);
    return -1;
  }
  HttpResponse *res = chttp_handle_request(req, pub_dir);
  if (res == NULL) {
    chttp_request_free(req);
    arena_free(&connection_arena);
    return -1;
  }
  size_t res_len = 0;
  char *raw_res = chttp_serialise_response(res, &res_len);
  if (raw_res == NULL) {
    chttp_request_free(req);
    chttp_response_free(res);
    arena_free(&connection_arena);
    return -1;
  }
  send(client_fd, raw_res, res_len, 0);

  chttp_request_free(req);
  chttp_response_free(res);
  arena_free(&connection_arena);
  return 0;
}

int chttp_listen_and_serve(HttpConfig *config) {
  int server_fd = setup_listener(config->port);
  if (server_fd < 0)
    return -1;

  while (1) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (client_fd >= 0) {
      chttp_handle_connection(client_fd, config->public_dir);
      close(client_fd);
    }
  }
  return 0;
}
