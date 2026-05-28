#include "chttp/server.h"
#include "clib/test_framework.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_KEEP_ALIVE_DEFAULT 0 // Do not keep connection alive
#define TEST_CONN_TIMEOUT 0

void setup_test_dir() { mkdir("test_www", 0777); }

void teardown_test_dir() { rmdir("test_www"); }

void setup_test_index_mock() {
  FILE *f = fopen("test_www/index.html", "w");
  if (f != NULL) {
    fprintf(f, "<h1>Testing</h1>");
    fclose(f);
  }
}

void teardown_test_index_mock() { unlink("test_www/index.html"); }

void test_handle_connection_sends_valid_http(Arena *a) {
  int fds[2];

  ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0,
         "Should be able to create socketpair");

  int server_side = fds[0];
  int client_side = fds[1];

  const char *request = "GET /index.html HTTP/1.1\r\n\r\n";
  write(client_side, request, strlen(request));

  chttp_handle_connection(server_side, "test_www", NULL, a,
                          TEST_KEEP_ALIVE_DEFAULT, TEST_CONN_TIMEOUT);

  char response[1024] = {0};
  ssize_t bytes_read = read(client_side, response, sizeof(response) - 1);
  ASSERT(bytes_read > 0, "Server should send data back");

  ASSERT(strstr(response, "HTTP/1.1 200 OK") != NULL,
         "Response should contain 200 OK status line");
  ASSERT(strstr(response, "Content-Length:") != NULL,
         "Response should contain Content-Length header");
  ASSERT(strstr(response, "<h1>Testing</h1>") != NULL,
         "Response body should contain the expected text");

  close(server_side);
  close(client_side);
}

void test_handle_connection_invalid_request(Arena *a) {
  int fds[2];

  socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  int server_side = fds[0];
  int client_side = fds[1];

  const char *bad_request = "GARBAGE_REQUEST_WITHOUT_HTTP_FORMAT\r\n\r\n";
  write(client_side, bad_request, strlen(bad_request));

  ASSERT_INT_EQ(
      chttp_handle_connection(server_side, "test_www", NULL, a,
                              TEST_KEEP_ALIVE_DEFAULT, TEST_CONN_TIMEOUT),
      -1, "Server should return -1 error status code for invalid request");
  close(server_side);
  close(client_side);
}

void test_handle_error_status_code_responses(Arena *a) {
  int fds[2];

  socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  int server_side = fds[0];
  int client_side = fds[1];

  const char *not_found_req = "GET /unknown.html HTTP/1.1\r\n\r\n";
  write(client_side, not_found_req, strlen(not_found_req));

  ASSERT_INT_EQ(
      chttp_handle_connection(server_side, "test_www", NULL, a,
                              TEST_KEEP_ALIVE_DEFAULT, TEST_CONN_TIMEOUT),
      0,
      "Server should return 0 (Handled) even when serving a 404 status code");

  char response[1024] = {0};
  read(client_side, response, sizeof(response) - 1);
  ASSERT_PTR_NOT_NULL(
      strstr(response, "HTTP/1.1 404 Not Found"),
      "Response protocol stream should specify 404 status layout");
  ASSERT_PTR_NOT_NULL(strstr(response, "Content-Type: text/html"),
                      "Response headers should retain HTML mime signatures");
  close(server_side);
  close(client_side);
}

int main() {
  setup_test_dir();
  setup_test_index_mock();

  Arena a;
  arena_init(&a, 4096);

  printf("\nRunning: %s\n", __FILE__);
  test_handle_connection_sends_valid_http(&a);
  test_handle_connection_invalid_request(&a);
  test_handle_error_status_code_responses(&a);
  test_summary();

  arena_free(&a);

  teardown_test_index_mock();
  teardown_test_dir();
  return tests_failed > 0 ? 1 : 0;
}
