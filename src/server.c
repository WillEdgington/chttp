#include "chttp/server.h"
#include "chttp/config.h"
#include "chttp/handler.h"
#include "chttp/http.h"
#include "chttp/logger.h"
#include "chttp/tpool.h"
#include "clib/arena.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 10
#define BUFFER_SIZE 4096           // 4 KB
#define CONNECTION_ARENA_SIZE 8192 // 8 KB
#define DEFAULT_THREAD_COUNT 4

char *reconstruct_request_line(HttpRequest *req) {
  const char *method_str = chttp_method_to_string(req->method);
  size_t line_len =
      snprintf(NULL, 0, "%s %s %s", method_str, req->path ? req->path : "/",
               req->version ? req->version : "HTTP/1.1");
  char *req_line_buf = arena_alloc(req->arena, line_len + 1);
  snprintf(req_line_buf, line_len + 1, "%s %s %s",
           chttp_method_to_string(req->method), req->path ? req->path : "/",
           req->version ? req->version : "HTTP/1.1");
  return req_line_buf;
}

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

int chttp_handle_connection(int client_fd, const char *pub_dir,
                            const char *client_ip) {
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
  if (res == NULL || res->body == NULL) {
    if (res != NULL)
      chttp_response_free(res);
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

  chttp_log_http(client_ip, reconstruct_request_line(req), res->status_code,
                 res_len);
  chttp_request_free(req);
  chttp_response_free(res);
  arena_free(&connection_arena);
  return 0;
}

int chttp_listen_and_serve(HttpConfig *config) {
  int server_fd = setup_listener(config->port);
  if (server_fd < 0) {
    LOG_ERROR("Failed to bind listener to port %d", config->port);
    return -1;
  }

  chttp_tpool_t *pool =
      chttp_tpool_create(DEFAULT_THREAD_COUNT, config->public_dir);
  if (pool == NULL) {
    LOG_ERROR("Failed to initialize system worker thread pool engine.");
    close(server_fd);
    return -1;
  }

  LOG_INFO("Thread pool engine spawned with %d active workers.",
           DEFAULT_THREAD_COUNT);

  while (1) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (client_fd >= 0) {
      char *client_ip = inet_ntoa(address.sin_addr);
      if (chttp_tpool_push(pool, client_fd, client_ip) != 0) {
        LOG_WARN("Task drop detected: server queue full or shutting down.");
        close(client_fd);
      }
    }
  }

  chttp_tpool_destroy(pool);
  close(server_fd);
  return 0;
}
