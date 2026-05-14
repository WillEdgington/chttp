#include "chttp/http.h"
#include "clib/arena.h"
#include "clib/hashmap.h"

#include <string.h>
#include <strings.h>

// string comparison for hashmap
static int str_compare(const void *k1, const void *k2, size_t key_size) {
  (void)key_size;
  return strcmp(*(const char **)k1, *(const char **)k2);
}

// simple string hash (DJB2)
static size_t str_hash(const void *key, size_t key_size) {
  (void)key_size;
  const char *str = *(const char **)key;
  size_t hash = 5381;
  int c;
  while ((c = *str++))
    hash = ((hash << 5) + hash) + c;
  return hash;
}

static int http_request_init(HttpRequest *request, Arena *arena) {
  if (hashmap_init(request->headers, sizeof(char *), sizeof(char *), arena) !=
      0)
    return -1;
  hashmap_set_functions(request->headers, str_hash, str_compare);
  request->arena = arena;
  return 0;
}

static char *arena_strndup(Arena *arena, const char *src, size_t n) {
  char *dest = arena_alloc(arena, n + 1);
  if (dest) {
    memcpy(dest, src, n);
    dest[n] = '\0';
  }
  return dest;
}

static char *parse_header(HttpRequest *req, const char *ptr) {
  const char *colon = strchr(ptr, ':');
  const char *end = strstr(ptr, "\r\n");
  if (!colon || !end)
    return NULL;

  char *key = arena_strndup(req->arena, ptr, colon - ptr);
  const char *val_start = colon + 1;
  while (*val_start == ' ')
    val_start++;

  char *val = arena_strndup(req->arena, val_start, end - val_start);
  hashmap_put(req->headers, &key, &val);
  return (char *)end + 2;
}

HttpMethod chttp_method_from_string(const char *method_str) {
  if (strcmp(method_str, "GET") == 0)
    return HTTP_METHOD_GET;
  if (strcmp(method_str, "POST") == 0)
    return HTTP_METHOD_POST;
  return HTTP_METHOD_UNKNOWN;
}

const char *chttp_method_to_string(HttpMethod method) {
  switch (method) {
  case HTTP_METHOD_GET:
    return "GET";
  case HTTP_METHOD_POST:
    return "POST";
  default:
    return "UNKNOWN";
  }
}

HttpRequest *chttp_parse_request(const char *raw_data, Arena *arena) {
  HttpRequest *req = arena_alloc(arena, sizeof(HttpRequest));
  req->headers = arena_alloc(arena, sizeof(HashMap));
  if (http_request_init(req, arena) != 0)
    return NULL;

  const char *line_start = raw_data;
  const char *next_space;

  // Method
  next_space = strchr(line_start, ' ');
  if (!next_space)
    return NULL;
  char *method_tmp = arena_strndup(arena, line_start, next_space - line_start);
  req->method = chttp_method_from_string(method_tmp);
  line_start = next_space + 1;

  // Path
  next_space = strchr(line_start, ' ');
  if (!next_space)
    return NULL;
  req->path = arena_strndup(arena, line_start, next_space - line_start);
  line_start = next_space + 1;

  // Version
  const char *line_end = strstr(line_start, "\r\n");
  if (!line_end)
    return NULL;
  req->version = arena_strndup(arena, line_start, line_end - line_start);
  line_start = line_end + 2;

  // Headers
  while (strncmp(line_start, "\r\n", 2) != 0 && *line_start != '\0') {
    line_start = parse_header(req, line_start);
    if (line_start == NULL)
      break;
  }
  return req;
}

void chttp_request_free(HttpRequest *request) {
  hashmap_free(request->headers);
}