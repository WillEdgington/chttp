#include "chttp/http.h"
#include "clib/arena.h"
#include "clib/hashmap.h"

#include <stdio.h>
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
  request->headers = arena_alloc(arena, sizeof(HashMap));
  if (request->headers == NULL || hashmap_init(request->headers, sizeof(char *),
                                               sizeof(char *), arena) != 0)
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

static char *consume_token(const char **cursor, const char *delim,
                           Arena *arena) {
  const char *start = *cursor;
  const char *end = strstr(start, delim);
  if (!end)
    return NULL;

  char *token = arena_strndup(arena, start, end - start);
  *cursor = end + strlen(delim);
  return token;
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
  if (http_request_init(req, arena) != 0)
    return NULL;

  const char *cursor = raw_data;

  // Method
  char *method_str = consume_token(&cursor, " ", arena);
  if (!method_str)
    return NULL;
  req->method = chttp_method_from_string(method_str);

  // Path
  req->path = consume_token(&cursor, " ", arena);
  if (!req->path)
    return NULL;

  // Version
  req->version = consume_token(&cursor, "\r\n", arena);
  if (!req->version)
    return NULL;

  // Headers
  while (strncmp(cursor, "\r\n", 2) != 0 && *cursor != '\0') {
    cursor = parse_header(req, cursor);
    if (cursor == NULL)
      break;
  }
  return req;
}

void chttp_request_free(HttpRequest *request) {
  hashmap_free(request->headers);
}

int chttp_response_init(HttpResponse *res, Arena *arena) {
  res->headers = arena_alloc(arena, sizeof(HashMap));
  if (res->headers == NULL ||
      hashmap_init(res->headers, sizeof(char *), sizeof(char *), arena) != 0)
    return -1;
  hashmap_set_functions(res->headers, str_hash, str_compare);

  res->arena = arena;
  res->status_code = 200;
  res->status_message = "OK";
  res->body = NULL;
  res->body_len = 0;
  return 0;
}

char *chttp_serialise_response(HttpResponse *res, size_t *out_len) {
  size_t metadata_cap = 8192; // will calculate properly at some point, but 8 KB
                              // should be safe for now
  char *buffer = arena_alloc(res->arena, metadata_cap + res->body_len);
  if (buffer == NULL)
    return NULL;

  size_t offset = 0;

  offset +=
      snprintf(buffer + offset, metadata_cap - offset, "HTTP/1.1 %d %s\r\n",
               res->status_code, res->status_message);

  Iter it = hashmap_iter(res->headers);
  while (it.next(&it)) {
    char *key = *(char **)it.current.key;
    char *val = *(char **)it.current.value;
    offset += snprintf(buffer + offset, metadata_cap - offset, "%s: %s\r\n",
                       key, val);
  }

  offset += snprintf(buffer + offset, metadata_cap - offset,
                     "Content-Length: %zu\r\n", res->body_len);
  offset += snprintf(buffer + offset, metadata_cap - offset, "\r\n");

  if (res->body != NULL && res->body_len > 0) {
    memcpy(buffer + offset, res->body, res->body_len);
    offset += res->body_len;
  }
  *out_len = offset;
  return buffer;
}

void chttp_response_free(HttpResponse *response) {
  hashmap_free(response->headers);
}
