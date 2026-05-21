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
  ASSERT_PTR_NOT_NULL(req, "Parser should not return NULL for valid request");
  ASSERT(req->method == HTTP_METHOD_GET, "Method should be GET");
  ASSERT_STR_EQ(req->path, "/index.html", "Path should be /index.html");
  ASSERT_STR_EQ(req->version, "HTTP/1.1", "Version should be HTTP/1.1");

  const char *key = "Host";
  char *host = *(char **)hashmap_get(req->headers, &key);
  ASSERT_PTR_NOT_NULL(host, "Host header should not be missing");
  ASSERT_STR_EQ(host, "localhost", "Host value should be correct (localhost)");

  chttp_request_free(req);
  arena_free(&a);
}

void test_parse_unsupported_method() {
  Arena a;
  arena_init(&a, 1024); // 1KB

  const char *raw = "DELETE /unsupported HTTP/1.1\r\n\r\n";
  HttpRequest *req = chttp_parse_request(raw, &a);

  ASSERT_PTR_NOT_NULL(req, "Should return req with UNKNOWN method");
  ASSERT(req->method == HTTP_METHOD_UNKNOWN, "Method should be UNKNOWN");

  chttp_request_free(req);
  arena_free(&a);
}

void test_serialise_basic_response() {
  Arena a;
  arena_init(&a, 1024); // 1 KB

  HttpResponse res = {0};
  chttp_response_init(&res, &a);

  const char *key = "Host";
  const char *val = "localhost";
  hashmap_put(res.headers, &key, &val);

  res.status_code = 200;
  res.status_message = "OK";
  res.body = "Testing...";
  res.body_len = 10;

  size_t len;
  char *raw = chttp_serialise_response(&res, &len);
  ASSERT_PTR_NOT_NULL(strstr(raw, "HTTP/1.1 200 OK"),
                      "Status line should be in serialised response");
  ASSERT_PTR_NOT_NULL(strstr(raw, "Host: localhost"),
                      "Headers should be serialised");

  chttp_response_free(&res);
  arena_free(&a);
}

int main() {
  printf("\nRunning: %s...\n", __FILE__);
  test_parse_valid_get_request();
  test_parse_unsupported_method();
  test_serialise_basic_response();
  test_summary();
  return tests_failed > 0 ? 1 : 0;
}