#include "chttp/server.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 10
#define BUFFER_SIZE 4096 // 4 KB

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

void chttp_handle_connection(int client_fd) {
  char buffer[BUFFER_SIZE] = {0};

  ssize_t valread = recv(client_fd, buffer, sizeof(buffer), 0);
  if (valread <= 0)
    return;

  const char *response =
      "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
  send(client_fd, response, strlen(response), 0);
}

int chttp_listen_and_serve(HttpConfig config) {
  int server_fd = setup_listener(config.port);
  if (server_fd < 0)
    return -1;

  while (1) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (client_fd >= 0) {
      chttp_handle_connection(client_fd);
      close(client_fd);
    }
  }
  return 0;
}
