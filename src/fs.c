#define _DEFAULT_SOURCE // Necessary for realpath on many Linux systems
#include "chttp/fs.h"
#include "clib/arena.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *arena_strndup(Arena *arena, const char *src, size_t n) {
  char *dest = arena_alloc(arena, n + 1);
  if (dest != NULL) {
    memcpy(dest, src, n);
    dest[n] = '\0';
  }
  return dest;
}

char *chttp_resolve_path(const char *request_path, const char *base_dir,
                         Arena *arena) {
  char *abs_base = realpath(base_dir, NULL);
  if (!abs_base)
    return NULL;

  char raw_path[PATH_MAX];
  snprintf(raw_path, sizeof(raw_path), "%s/%s", abs_base, request_path);

  char *resolved = realpath(raw_path, NULL);
  if (resolved == NULL) {
    free(abs_base);
    return NULL;
  }

  char *result = NULL;
  if (strncmp(resolved, abs_base, strlen(abs_base)) == 0) {
    result = arena_strndup(arena, resolved, strlen(resolved));
  }

  free(abs_base);
  free(resolved);
  return result;
}

char *chttp_read_file(const char *full_path, Arena *arena, size_t *out_size) {
  FILE *f = fopen(full_path, "rb");
  if (f == NULL)
    return NULL;

  fseek(f, 0, SEEK_END);
  *out_size = ftell(f);
  rewind(f);

  char *buffer = arena_alloc(arena, *out_size);
  if (buffer != NULL) {
    fread(buffer, 1, *out_size, f);
    buffer[*out_size] = '\0';
  }
  fclose(f);
  return buffer;
}
