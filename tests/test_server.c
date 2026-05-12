#include "chttp/server.h"
#include "clib/test_framework.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void test_handle_connection_sends_valid_http() {
  int fds[2];

  ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0,
         "Failed to create socketpair");

  int server_side = fds[0];
  int client_side = fds[1];

  const char *request = "GET / HTTP/1.1\r\n\r\n";
  write(client_side, request, strlen(request));

  chttp_handle_connection(server_side);

  char response[1024] = {0};
  ssize_t bytes_read = read(client_side, response, sizeof(response) - 1);
  ASSERT(bytes_read > 0, "Server sent no data back");

  ASSERT(strstr(response, "HTTP/1.1 200 OK") != NULL,
         "Response missing 200 OK status line");
  ASSERT(strstr(response, "Content-Length:") != NULL,
         "Response missing Content-Length header");
  ASSERT(strstr(response, "Hello, World!") != NULL,
         "Response body does not contain expected text");

  close(server_side);
  close(client_side);
}

int main() {
  printf("\nRunning: %s\n", __FILE__);
  test_handle_connection_sends_valid_http();
  test_summary();
  return tests_failed > 0 ? 1 : 0;
}
