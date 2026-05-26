#include "chttp/handler.h"
#include "chttp/err.h"
#include "chttp/fs.h"
#include "chttp/http.h"
#include "clib/hashmap.h"
#include <stdio.h>
#include <string.h>

static const char *resolve_pretty_path(const char *path, Arena *arena) {
  size_t len = strlen(path);
  const char *last_slash = strrchr(path, '/');
  const char *last_dot = strrchr(path, '.');

  if (len > 0 && path[len - 1] == '/') { // add index.html
    size_t suffix_len = strlen("index.html");
    char *new_path = arena_alloc(arena, len + suffix_len + 1);
    if (new_path == NULL)
      return NULL;
    snprintf(new_path, len + suffix_len + 1, "%sindex.html", path);
    path = new_path;
  } else if (last_dot == NULL ||
             (last_slash != NULL && last_dot < last_slash)) { // add /index.html
    size_t suffix_len = strlen("/index.html");
    char *new_path = arena_alloc(arena, len + suffix_len + 1);
    if (new_path == NULL)
      return NULL;
    snprintf(new_path, len + suffix_len + 1, "%s/index.html", path);
    path = new_path;
  }
  return path;
}

typedef void (*MethodHandler)(HttpRequest *req, const char *base_dir,
                              HttpResponse *res);

static void handle_get(HttpRequest *req, const char *base_dir,
                       HttpResponse *res) {
  const char *target_path = resolve_pretty_path(req->path, req->arena);
  if (target_path == NULL) {
    chttp_create_error_response(res, 500, "Internal Server Error", base_dir);
    return;
  }

  char *full_path = chttp_resolve_path(target_path, base_dir, req->arena);
  if (full_path == NULL) {
    chttp_create_error_response(res, 404, "Not Found", base_dir);
    return;
  }

  size_t file_size = 0;
  char *file_bytes = chttp_read_file(full_path, res->arena, &file_size);
  if (file_bytes == NULL) {
    chttp_create_error_response(res, 404, "Not Found", base_dir);
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

static void handle_unknown(HttpRequest *req, const char *base_dir,
                           HttpResponse *res) {
  (void)req;
  chttp_create_error_response(res, 405, "Method Not Allowed", base_dir);
}

static const MethodHandler method_handlers[] = {
    [HTTP_METHOD_GET] = handle_get, [HTTP_METHOD_UNKNOWN] = handle_unknown};

HttpResponse *chttp_handle_request(HttpRequest *req, const char *base_dir) {
  if (req == NULL)
    return NULL;

  HttpResponse *res = arena_alloc(req->arena, sizeof(HttpResponse));
  if (res == NULL || chttp_response_init(res, req->arena) != 0)
    return NULL;

  MethodHandler handler = method_handlers[req->method];
  if (handler != NULL)
    handler(req, base_dir, res);
  return res;
}
