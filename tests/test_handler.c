#include "chttp/handler.h"
#include "chttp/http.h"
#include "clib/arena.h"
#include "clib/hashmap.h"
#include "clib/test_framework.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

void test_handle_valid_get(Arena *a) {
  const char *raw_req = "GET /index.html HTTP/1.1\r\n"
                        "Host: localhost\r\n"
                        "User-Agent: test-agent\r\n"
                        "\r\n";
  HttpRequest *req = chttp_parse_request(raw_req, a);
  HttpResponse *res = chttp_handle_request(req, "test_www");

  ASSERT_INT_EQ(res->status_code, 200, "Status code of 200 for valid request");
  ASSERT_STR_EQ(res->status_message, "OK",
                "Status message OK for valid request");
  ASSERT_INT_EQ(res->body_len, 16, "Body length should not mismatch");
  ASSERT_STR_EQ(res->body, "<h1>Testing</h1>",
                "Body content should not mismatch");

  char *key = "Content-Type";
  char **val = (char **)hashmap_get(res->headers, &key);
  ASSERT_PTR_NOT_NULL(val, "Content-Type header should be in response");
  ASSERT_STR_EQ(*val, "text/html", "MIME type for .html should be text/html");

  chttp_request_free(req);
  chttp_response_free(res);
}

void test_handle_index_fallback(Arena *a) {
  const char *raw_req = "GET / HTTP/1.1\r\n"
                        "Host: localhost\r\n"
                        "User-Agent: test-agent\r\n"
                        "\r\n";
  HttpRequest *req = chttp_parse_request(raw_req, a);
  HttpResponse *res = chttp_handle_request(req, "test_www");
  ASSERT_INT_EQ(res->status_code, 200,
                "Root path should resolve to index.html");
  chttp_request_free(req);
  chttp_response_free(res);
}

void test_handle_not_found(Arena *a) {
  const char *raw_req = "GET /missing.html HTTP/1.1\r\n"
                        "Host: localhost\r\n"
                        "User-Agent: test-agent\r\n"
                        "\r\n";
  HttpRequest *req = chttp_parse_request(raw_req, a);
  HttpResponse *res = chttp_handle_request(req, "test_www");
  ASSERT_INT_EQ(res->status_code, 404, "Non-existent file should return 404");
  chttp_request_free(req);
  chttp_response_free(res);
}

void test_handle_invalid_method(Arena *a) {
  const char *raw_req = "DELETE /index.html HTTP/1.1\r\n"
                        "Host: localhost\r\n"
                        "User-Agent: test-agent\r\n"
                        "\r\n";
  HttpRequest *req = chttp_parse_request(raw_req, a);
  HttpResponse *res = chttp_handle_request(req, "test_www");
  ASSERT_INT_EQ(res->status_code, 405,
                "Unimplemented methods should return 405");
  chttp_request_free(req);
  chttp_response_free(res);
}

int main() {
  Arena a;
  arena_init(&a, 4096); // 4 KB

  setup_test_dir();
  setup_test_index_mock();

  printf("\nRunning: %s...\n", __FILE__);
  test_handle_valid_get(&a);
  test_handle_index_fallback(&a);
  test_handle_not_found(&a);
  test_handle_invalid_method(&a);
  test_summary();

  teardown_test_index_mock();
  teardown_test_dir();
  arena_free(&a);
  return tests_failed > 0 ? 1 : 0;
}