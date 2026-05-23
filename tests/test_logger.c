#include "chttp/logger.h"
#include "clib/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void setup_test_dir() { mkdir("test_dir", 0777); }

void teardown_test_dir() { rmdir("test_dir"); }

void teardown_test_log_mock(const char *filename) { unlink(filename); }

void read_file_content(const char *filename, char *buffer, size_t max_len) {
  FILE *f = fopen(filename, "r");
  if (f == NULL) {
    buffer[0] = '\0';
    return;
  }
  size_t bytes_read = fread(buffer, 1, max_len - 1, f);
  buffer[bytes_read] = '\0';
  fclose(f);
}

void test_logger_filtering() {
  const char *log_path = "test_dir/test_filter.log";

  int init_res = chttp_logger_init(log_path, LOG_LEVEL_INFO);
  ASSERT_INT_EQ(init_res, 0,
                "Logger should initialize successfully with a valid file path");

  LOG_DEBUG("This debug message should be filtered out completely");
  LOG_INFO("This info message should be recorded cleanly");
  LOG_ERROR("This error message should be recorded cleanly");

  chttp_logger_free();

  char buffer[1024];
  read_file_content(log_path, buffer, 1024);
  ASSERT_PTR_NULL(
      strstr(buffer, "debug message"),
      "Debug log must not exist when minimum level threshold is set to INFO");
  ASSERT_PTR_NOT_NULL(
      strstr(buffer, "info message"),
      "Info log must be preserved when minimum level threshold is set to INFO");
  ASSERT_PTR_NOT_NULL(strstr(buffer, "error message"),
                      "Error log must be preserved when minimum level "
                      "threshold is set to INFO");
  teardown_test_log_mock(log_path);
}

void test_logger_http_format() {
  const char *log_path = "test_dir/test_http.log";

  chttp_logger_init(log_path, LOG_LEVEL_INFO);
  chttp_log_http("192.168.1.50", "GET /static/style.css HTTP/1.1", 200, 4096);
  chttp_logger_free();

  char buffer[1024];
  read_file_content(log_path, buffer, 1024);
  ASSERT_PTR_NOT_NULL(
      strstr(buffer, "192.168.1.50"),
      "HTTP log line must begin with the correct client IP address");
  ASSERT_PTR_NOT_NULL(
      strstr(buffer, "\"GET /static/style.css HTTP/1.1\""),
      "HTTP log line must enclose the exact request line string inside quotes");
  ASSERT_PTR_NOT_NULL(strstr(buffer, "200 4096"),
                      "HTTP log line must terminate with the correct status "
                      "code and byte length");
  teardown_test_log_mock(log_path);
}

int main() {
  setup_test_dir();

  printf("\nRunning: %s...\n", __FILE__);
  test_logger_filtering();
  test_logger_http_format();
  test_summary();

  teardown_test_dir();
  return tests_failed > 0 ? 1 : 0;
}