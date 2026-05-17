#include "chttp/fs.h"
#include "clib/arena.h"
#include "clib/test_framework.h"
#include <sys/stat.h>
#include <unistd.h>

void setup_test_dir() { mkdir("test_www", 0777); }

void teardown_test_dir() { rmdir("test_www"); }

void setup_test_html_mock() {
  FILE *f = fopen("test_www/index.html", "w");
  if (f != NULL) {
    fprintf(f, "<h1>Testing</h1>");
    fclose(f);
  }
}

void teardown_test_html_mock() { unlink("test_www/index.html"); }

static const char mock_png_data[] = {'\x89', 'P',  'N',  'G',
                                     '\0',   '\0', '\r', '\n'};
static const size_t mock_png_size = sizeof(mock_png_data);

void setup_test_binary_mock() {
  FILE *f = fopen("test_www/image.png", "wb");
  if (f != NULL) {
    fwrite(mock_png_data, 1, mock_png_size, f);
    fclose(f);
  }
}

void teardown_test_binary_mock() { unlink("test_www/image.png"); }

void test_html_file(Arena *a) {
  setup_test_html_mock();

  char *path = chttp_resolve_path("/index.html", "test_www", a);
  ASSERT_PTR_NOT_NULL(path, "Should resolve valid html file");
  ASSERT_PTR_NOT_NULL(strstr(path, "test_www/index.html"),
                      "Path should not have mismatch");

  char *unsafe = chttp_resolve_path("/../../etc/passwords", "test_www", a);
  ASSERT_PTR_NULL(unsafe, "Should block directory traversal");

  size_t size;
  char *content = chttp_read_file(path, a, &size);

  ASSERT_PTR_NOT_NULL(content, "File should be readable");
  ASSERT_INT_EQ(size, 16, "Size should not have mismatch");
  ASSERT_STR_EQ(content, "<h1>Testing</h1>",
                "Content should not have mismatch");

  teardown_test_html_mock();
}

void test_binary_file(Arena *a) {
  setup_test_binary_mock();

  char *path = chttp_resolve_path("/image.png", "test_www", a);
  ASSERT_PTR_NOT_NULL(path, "Should resolve valid binary file");

  size_t size;
  char *content = chttp_read_file(path, a, &size);

  ASSERT_PTR_NOT_NULL(content, "Binary file should be readable");
  ASSERT_INT_EQ(size, mock_png_size, "Binary size should not have mismatch");

  ASSERT(memcmp(content, mock_png_data, mock_png_size) == 0,
         "Binary content should not be corrupted");

  teardown_test_binary_mock();
}

int main() {
  Arena a;
  arena_init(&a, 4096); // 4 KB
  setup_test_dir();

  printf("\nRunning: %s...\n", __FILE__);
  test_html_file(&a);
  test_binary_file(&a);
  test_summary();

  teardown_test_dir();
  arena_free(&a);
  return tests_failed > 0 ? 1 : 0;
}
