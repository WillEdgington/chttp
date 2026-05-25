#include "chttp/config.h"
#include "chttp/logger.h"
#include "clib/arena.h"
#include "clib/test_framework.h"
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

void setup_test_dir() { mkdir("test_dir", 0777); }

void teardown_test_dir() { rmdir("test_dir"); }

void setup_test_config_mock(const char *filename, const char *content) {
  FILE *f = fopen(filename, "w");
  if (f != NULL) {
    fputs(content, f);
    fclose(f);
  }
}

void teardown_test_config_mock(const char *filename) { unlink(filename); }

void test_perfect_config_toml(Arena *a) {
  const char *cfg = "[server]\n"
                    "port = 9000\n"
                    "public_dir = ./test_www";
  setup_test_config_mock("test_dir/test_perfect.toml", cfg);

  HttpConfig config = chttp_config_load("test_dir/test_perfect.toml", a);
  ASSERT_INT_EQ(config.port, 9000,
                "Should correctly parse and assign valid port number");
  ASSERT_STR_EQ(config.public_dir, "./test_www",
                "Should allocate and match public directory string");

  teardown_test_config_mock("test_dir/test_perfect.toml");
}

void test_noisy_config_toml(Arena *a) {
  const char *cfg = "  \n"
                    "random = ignore\n"
                    "[server]\n"
                    "# this is a comment\n"
                    "port = 9000\n"
                    "; semi-colon comment\n"
                    "public_dir = ./test_www\n"
                    "\n"
                    "[unknown]\n"
                    "this_wont_scan = yes\n";
  setup_test_config_mock("test_dir/test_noise.toml", cfg);

  HttpConfig config = chttp_config_load("test_dir/test_noise.toml", a);
  ASSERT_INT_EQ(config.port, 9000,
                "Should correctly parse port config through noisy toml file");
  ASSERT_STR_EQ(
      config.public_dir, "./test_www",
      "Should correctly parse public_dir config through noisy toml file");

  teardown_test_config_mock("test_dir/test_noise.toml");
}

void test_invalid_config_toml(Arena *a) {
  const char *cfg_alpha = "[server]\nport = 9t5hun";
  setup_test_config_mock("test_dir/test_bound.toml", cfg_alpha);

  HttpConfig config = chttp_config_load("test_dir/test_bound.toml", a);
  ASSERT_INT_EQ(
      config.port, 8080,
      "Should retain default port when input config contains non-digits");

  const char *cfg_overflow = "[server]\nport = 70000";
  setup_test_config_mock("test_dir/test_bound.toml", cfg_overflow);

  config = chttp_config_load("test_dir/test_bound.toml", a);
  ASSERT_INT_EQ(
      config.port, 8080,
      "Should retain default port when input config is exceeding 65535");

  teardown_test_config_mock("test_dir/test_bound.toml");
}

void test_missing_config_toml(Arena *a) {
  HttpConfig config = chttp_config_load("test_dir/non_existent_config.toml", a);

  ASSERT_INT_EQ(config.port, 8080,
                "Should retain default port when config toml is missing");
  ASSERT_STR_EQ(config.public_dir, ".",
                "Should retain default public_dir when config toml is missing");
}

void test_logging_config_toml(Arena *a) {
  const char *cfg = "[server]\n"
                    "log_level = ERROR\n"
                    "log_file = ./server.log";
  setup_test_config_mock("test_dir/test_logging.toml", cfg);

  HttpConfig config = chttp_config_load("test_dir/test_logging.toml", a);
  ASSERT_INT_EQ(config.log_level, LOG_LEVEL_ERROR,
                "Should correctly parse and assign log level");
  ASSERT_STR_EQ(config.log_filepath, "./server.log",
                "Should allocate and match log file path string");

  teardown_test_config_mock("test_dir/test_logging.toml");
}

int main() {
  Arena a;
  arena_init(&a, 4096); // 4 KB

  setup_test_dir();

  printf("\nRunning: %s...\n", __FILE__);
  test_perfect_config_toml(&a);
  test_noisy_config_toml(&a);
  test_invalid_config_toml(&a);
  test_missing_config_toml(&a);
  test_logging_config_toml(&a);
  test_summary();

  teardown_test_dir();
  arena_free(&a);
  return tests_failed > 0 ? 1 : 0;
}
