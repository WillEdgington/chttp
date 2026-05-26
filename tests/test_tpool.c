#include "chttp/tpool.h"
#include "clib/test_framework.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_THREAD_COUNT 4

void setup_test_dir() { mkdir("test_dir", 0777); }

void teardown_test_dir() { rmdir("test_dir"); }

void setup_test_index_mock() {
  FILE *f = fopen("test_dir/index.html", "w");
  if (f != NULL) {
    fprintf(f, "<h1>Testing Tpool</h1>");
    fclose(f);
  }
}

void teardown_test_index_mock() { unlink("test_dir/index.html"); }

void test_tpool_create_and_destroy() {
  chttp_tpool_t *pool = chttp_tpool_create(TEST_THREAD_COUNT, "test_dir");

  ASSERT_PTR_NOT_NULL(
      pool, "Thread pool initialization should return a valid pointer");
  ASSERT_INT_EQ(
      pool->thread_count, TEST_THREAD_COUNT,
      "Thread pool should allocate the exact requested worker capacity");

  chttp_tpool_destroy(pool);
}

void test_tpool_processes_request_asynchronously() {
  chttp_tpool_t *pool = chttp_tpool_create(TEST_THREAD_COUNT, "test_dir");
  ASSERT_PTR_NOT_NULL(pool, "Pool must boot cleanly to process tasks");

  int fds[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  int server_side = fds[0];
  int client_side = fds[1];

  const char *request = "GET /index.html HTTP/1.1\r\n\r\n";
  write(client_side, request, strlen(request));

  ASSERT_INT_EQ(chttp_tpool_push(pool, server_side, "127.0.0.1"), 0,
                "Task insertion into the worker pool queue should succeed");

  char response[1024] = {0};
  ssize_t bytes_read = read(client_side, response, sizeof(response) - 1);

  ASSERT(bytes_read > 0, "Worker thread should process task and write data "
                         "back across the socket boundary");
  ASSERT_PTR_NOT_NULL(
      strstr(response, "HTTP/1.1 200 OK"),
      "Asynchronous pool response must contain a valid 200 OK status line");
  ASSERT_PTR_NOT_NULL(strstr(response, "<h1>Testing Tpool</h1>"),
                      "Asynchronous worker payload must serve the requested "
                      "index file data cleanly");

  close(client_side);
  chttp_tpool_destroy(pool);
}

int main() {
  setup_test_dir();
  setup_test_index_mock();

  printf("\nRunning: %s\n", __FILE__);
  test_tpool_create_and_destroy();
  test_tpool_processes_request_asynchronously();
  test_summary();

  teardown_test_index_mock();
  teardown_test_dir();
  return tests_failed > 0 ? 1 : 0;
}