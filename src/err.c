#include "chttp/err.h"
#include "chttp/fs.h"
#include "clib/hashmap.h"
#include <stdio.h>
#include <string.h>

#define CODE_BUF_LEN 12
#define CODE_SYN_LEN 13
#define MSG_SYN_LEN 16

static const char *DEFAULT_ERROR_TEMPLATE =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head><title>{status_code} {status_message}</title></head>\n"
    "<body>\n"
    "  <h1>{status_code} {status_message}</h1>\n"
    "</body>\n"
    "</html>\0";

static size_t count_substring(const char *str, const char *sub) {
  size_t count = 0;
  size_t sub_len = strlen(sub);
  const char *pos = str;

  while ((pos = strstr(pos, sub)) != NULL) {
    count++;
    pos += sub_len;
  }
  return count;
}

static char *interpolate_template(Arena *arena, const char *tmpl,
                                  size_t tmpl_len, int code, const char *msg,
                                  size_t *out_len) {
  char code_str[CODE_BUF_LEN];
  snprintf(code_str, CODE_BUF_LEN, "%d", code);

  size_t clen = strlen(code_str);
  size_t mlen = strlen(msg);

  size_t code_count = count_substring(tmpl, "{status_code}");
  size_t msg_count = count_substring(tmpl, "{status_message}");

  long size_delta = (long)(code_count * (clen - CODE_SYN_LEN)) +
                    (long)(msg_count * (mlen - MSG_SYN_LEN));
  size_t final_size = (size_t)((long)tmpl_len + size_delta + 1);

  char *dest = arena_alloc(arena, final_size);
  if (dest == NULL)
    return NULL;

  char *ptr = dest;
  const char *src = tmpl;

  while (*src != '\0') {
    if (strncmp(src, "{status_code}", CODE_SYN_LEN) == 0) {
      memcpy(ptr, code_str, clen);
      ptr += clen;
      src += CODE_SYN_LEN;
    } else if (strncmp(src, "{status_message}", MSG_SYN_LEN) == 0) {
      memcpy(ptr, msg, mlen);
      ptr += mlen;
      src += MSG_SYN_LEN;
    } else {
      *ptr++ = *src++;
    }
  }
  *ptr = '\0';
  *out_len = (size_t)(ptr - dest);
  return dest;
}

void chttp_create_error_response(HttpResponse *res, int code, const char *msg,
                                 const char *base_dir) {
  res->status_code = code;
  res->status_message = msg;

  char *header_key = "Content-Type";
  char *header_val = "text/html";
  hashmap_put(res->headers, &header_key, &header_val);

  const char *template_bytes;
  size_t template_len;
  char *err_path = chttp_resolve_path("/err.html.srv", base_dir, res->arena);
  if (err_path == NULL) {
    template_bytes = DEFAULT_ERROR_TEMPLATE;
    template_len = strlen(DEFAULT_ERROR_TEMPLATE);
  } else {
    template_len = 0;
    template_bytes = chttp_read_file(err_path, res->arena, &template_len);

    if (template_bytes == NULL || template_len == 0) {
      template_bytes = DEFAULT_ERROR_TEMPLATE;
      template_len = strlen(DEFAULT_ERROR_TEMPLATE);
    }
  }

  size_t final_len = 0;
  char *final_body = interpolate_template(res->arena, template_bytes,
                                          template_len, code, msg, &final_len);

  if (final_body != NULL) {
    res->body = final_body;
    res->body_len = final_len;
  }
}
