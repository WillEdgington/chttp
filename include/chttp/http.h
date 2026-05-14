#ifndef CHTTP_HTTP_H
#define CHTTP_HTTP_H

#include "clib/arena.h"
#include "clib/hashmap.h"

typedef enum {
  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
  HTTP_METHOD_UNKNOWN
} HttpMethod;

typedef struct {
  HttpMethod method;
  char *path;
  char *version;
  HashMap *headers;
  Arena *arena;
} HttpRequest;

HttpMethod chttp_method_from_string(const char *method_str);
const char *chttp_method_to_string(HttpMethod method);
HttpRequest *chttp_parse_request(const char *raw_data, Arena *arena);
void chttp_request_free(HttpRequest *request);

#endif