#include "chttp/handler.h"
#include "chttp/fs.h"
#include "chttp/http.h"
#include "clib/hashmap.h"
#include <string.h>

typedef void (*MethodHandler)(HttpRequest *req, const char *base_dir,
                              HttpResponse *res);

static void handle_get(HttpRequest *req, const char *base_dir,
                       HttpResponse *res) {
  const char *target_path = req->path;

  if (strcmp(target_path, "/") == 0)
    target_path = "/index.html";

  char *full_path = chttp_resolve_path(target_path, base_dir, req->arena);
  if (full_path == NULL) {
    res->status_code = 404;
    res->status_message = "Not Found";
    return;
  }

  size_t file_size = 0;
  char *file_bytes = chttp_read_file(full_path, res->arena, &file_size);
  if (file_bytes == NULL) {
    res->status_code = 404;
    res->status_message = "Not Found";
    return;
  }

  res->status_code = 200;
  res->status_message = "OK";
  res->body = file_bytes;
  res->body_len = file_size;

  const char *mime = chttp_get_mime_type(full_path);
  char *header_key = "Content-Type";
  char *header_val = (char *)mime;
  hashmap_put(res->headers, &header_key, &header_val);
}

static const MethodHandler method_handlers[] = {
    [HTTP_METHOD_GET] = handle_get,
};

HttpResponse *chttp_handle_request(HttpRequest *req, const char *base_dir) {
  if (req == NULL)
    return NULL;

  HttpResponse *res = arena_alloc(req->arena, sizeof(HttpResponse));
  if (res == NULL || chttp_response_init(res, req->arena) != 0)
    return NULL;

  if (req->method != HTTP_METHOD_UNKNOWN) {
    MethodHandler handler = method_handlers[req->method];
    if (handler != NULL)
      handler(req, base_dir, res);
  }
  return res;
}