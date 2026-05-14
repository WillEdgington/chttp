#include "chttp/http.h"
#include "clib/arena.h"
#include "clib/test_framework.h"

#include <string.h>

void test_parse_valid_get_request() {
  Arena a;
  arena_init(&a, 1024); // 1 KB

  const char *raw = "GET /index.html HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "User-Agent: test-agent\r\n"
                    "\r\n";
  HttpRequest *req = chttp_parse_request(raw, &a);
  ASSERT(req != NULL, "Parser returned NULL for valid request");
  ASSERT(req->method == HTTP_METHOD_GET, "Method should be GET");
  ASSERT(strcmp(req->path, "/index.html") == 0, "Path mismatch");
  ASSERT(strcmp(req->version, "HTTP/1.1") == 0, "Version mismatch");

  const char *key = "Host";
  char *host = *(char **)hashmap_get(req->headers, &key);
  ASSERT(host != NULL, "Host header missing");
  ASSERT(strcmp(host, "localhost") == 0, "Host value incorrect");

  chttp_request_free(req);
  arena_free(&a);
}

void test_parse_unsupported_method() {
  Arena a;
  arena_init(&a, 1024);

  const char *raw = "DELETE /unsupported HTTP/1.1\r\n\r\n";
  HttpRequest *req = chttp_parse_request(raw, &a);

  ASSERT(req != NULL, "Should return req with UNKNOWN method");
  ASSERT(req->method == HTTP_METHOD_UNKNOWN, "Method should be UNKNOWN");

  chttp_request_free(req);
  arena_free(&a);
}

int main() {
  printf("\nRunning: %s...\n", __FILE__);
  test_parse_valid_get_request();
  test_parse_unsupported_method();
  test_summary();
  return tests_failed > 0 ? 1 : 0;
}