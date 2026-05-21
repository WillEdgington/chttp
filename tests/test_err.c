#include "chttp/err.h"
#include "chttp/http.h"
#include "clib/arena.h"
#include "clib/test_framework.h"
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

void setup_test_dir() { mkdir("test_www", 0777); }

void teardown_test_dir() { rmdir("test_www"); }

void setup_test_err_mock() {
  FILE *f = fopen("test_www/err.html.srv", "w");
  if (f != NULL) {
    fprintf(f, "<p>Error Code: {status_code}</p>\n"
               "<h1>{status_message}</h1>\n"
               "<span>Code again: {status_code}</span>");
    fclose(f);
  }
}

void teardown_test_err_mock() { unlink("test_www/err.html.srv"); }

void test_error_template_interpolation(Arena *a) {
  setup_test_err_mock();
  HttpResponse res;
  chttp_response_init(&res, a);

  chttp_create_error_response(&res, 405, "Method Not Allowed", "test_www");

  ASSERT_INT_EQ(res.status_code, 405, "Response should preserve status code");
  ASSERT_STR_EQ(res.status_message, "Method Not Allowed",
                "Response should map raw message description");
  ASSERT_PTR_NOT_NULL(
      res.body,
      "Interpolation target should output valid generated output layouts");

  ASSERT_PTR_NOT_NULL(
      strstr(res.body, "Error Code: 405"),
      "First instance of {status_code} should be interpreted cleanly");
  ASSERT_PTR_NOT_NULL(
      strstr(res.body, "Code again: 405"),
      "Second instance of {status_code} should be interpreted cleanly");
  ASSERT_PTR_NOT_NULL(
      strstr(res.body, "<h1>Method Not Allowed</h1>"),
      "Target token {status_message} should be interpreted cleanly");

  teardown_test_err_mock();
  chttp_response_free(&res);
}

void test_error_fallback_interpolation(Arena *a) {
  HttpResponse res;
  chttp_response_init(&res, a);

  chttp_create_error_response(&res, 405, "Method Not Allowed", "test_www");
  ASSERT_PTR_NOT_NULL(
      res.body,
      "Body should fallback to default if err.html.srv could not be loaded");

  chttp_response_free(&res);
}

int main() {
  Arena a;
  arena_init(&a, 4096); // 4 KB

  setup_test_dir();

  printf("\nRunning: %s...\n", __FILE__);
  test_error_template_interpolation(&a);
  test_error_fallback_interpolation(&a);
  test_summary();

  teardown_test_dir();
  arena_free(&a);
  return tests_failed > 0 ? 1 : 0;
}