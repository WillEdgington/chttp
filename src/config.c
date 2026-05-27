#include "chttp/config.h"
#include "chttp/fs.h"
#include "chttp/logger.h"
#include "clib/arena.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef void (*kvAssign)(HttpConfig *config, const char *key, const char *val,
                         Arena *arena);

static char *trim_whitespace(char *str) {
  while (isspace((unsigned char)*str))
    str++;
  if (*str == '\0' || *str == '#')
    return str + strlen(str);

  char *end = str + strlen(str) - 1;

  char *hsh = strchr(str, '#');
  char *scol = strchr(str, ';');
  if (hsh != NULL || scol != NULL) {
    if (hsh == NULL) {
      end = scol - 1;
    } else if (scol == NULL) {
      end = hsh - 1;
    } else
      end = hsh < scol ? hsh - 1 : scol - 1;
  }

  while (end > str && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return str;
}

static void assign_port(HttpConfig *config, const char *val) {
  if (val == NULL || *val == '\0')
    return;

  for (int i = 0; val[i] != '\0'; i++)
    if (!isdigit((unsigned char)val[i]))
      return;

  int parsed_port = atoi(val);
  if (parsed_port > 0 && parsed_port <= 65535)
    config->port = parsed_port;
}

static void assign_public_dir(HttpConfig *config, const char *val,
                              Arena *arena) {
  if (val == NULL || *val == '\0')
    return;

  size_t len = strlen(val);
  char *dir_copy = arena_alloc(arena, len + 1);
  if (dir_copy != NULL) {
    memcpy(dir_copy, val, len + 1);
    config->public_dir = dir_copy;
  }
}

static void assign_log_level(HttpConfig *config, const char *val) {
  if (val == NULL || *val == '\0')
    return;

  if (strcmp(val, "DEBUG") == 0 || strcmp(val, "0") == 0) {
    config->log_level = LOG_LEVEL_DEBUG;
  } else if (strcmp(val, "INFO") == 0 || strcmp(val, "1") == 0) {
    config->log_level = LOG_LEVEL_INFO;
  } else if (strcmp(val, "WARN") == 0 || strcmp(val, "2") == 0) {
    config->log_level = LOG_LEVEL_WARN;
  } else if (strcmp(val, "ERROR") == 0 || strcmp(val, "3") == 0) {
    config->log_level = LOG_LEVEL_ERROR;
  }
}

static void assign_log_filepath(HttpConfig *config, const char *val,
                                Arena *arena) {
  if (val == NULL || *val == '\0')
    return;

  size_t len = strlen(val);
  char *path_copy = arena_alloc(arena, len + 1);
  if (path_copy != NULL) {
    memcpy(path_copy, val, len + 1);
    config->log_filepath = path_copy;
  }
}

static void assign_thread_count(HttpConfig *config, const char *val) {
  if (val == NULL || *val == '\0')
    return;

  for (int i = 0; val[i] != '\0'; i++)
    if (!isdigit((unsigned char)val[i]))
      return;

  int parsed_tc = atoi(val);
  if (parsed_tc > 0)
    config->thread_count = parsed_tc;
}

static void assign_worker_arena_size(HttpConfig *config, const char *val) {
  if (val == NULL || *val == '\0')
    return;

  for (int i = 0; val[i] != '\0'; i++)
    if (!isdigit((unsigned char)val[i]))
      return;

  int parsed_was = atoi(val);
  if (parsed_was > 0)
    config->worker_arena_size = parsed_was;
}

static void resolve_server_key_value_pair(HttpConfig *config, const char *key,
                                          const char *val, Arena *arena) {
  if (strcmp(key, "port") == 0) {
    assign_port(config, val);
  } else if (strcmp(key, "public_dir") == 0) {
    assign_public_dir(config, val, arena);
  } else if (strcmp(key, "log_level") == 0) {
    assign_log_level(config, val);
  } else if (strcmp(key, "log_file") == 0) {
    assign_log_filepath(config, val, arena);
  } else if (strcmp(key, "thread_count") == 0) {
    assign_thread_count(config, val);
  } else if (strcmp(key, "worker_arena_size") == 0) {
    assign_worker_arena_size(config, val);
  }
}

static char *resolve_config_subsection(HttpConfig *config, const char *content,
                                       Arena *arena, kvAssign assign_func) {
  const char *cur = content;

  while (*cur != '\0') {
    const char *peek = cur;
    while (*peek != '\0' && *peek != '\n' && isspace((unsigned char)*peek))
      peek++;
    if (*peek == '[')
      return (char *)peek;

    const char *next_line = strchr(cur, '\n');
    size_t line_len = next_line ? (size_t)(next_line - cur) : strlen(cur);
    if (line_len > 0 && assign_func != NULL) {
      char line_buf[line_len + 1];
      memcpy(line_buf, cur, line_len);
      line_buf[line_len] = '\0';

      char *trimmed = trim_whitespace(line_buf);
      if (trimmed[0] != '\0') {
        char *eq = strchr(trimmed, '=');
        if (eq != NULL) {
          *eq = '\0';
          char *key = trim_whitespace(trimmed);
          char *val = trim_whitespace(eq + 1);
          assign_func(config, key, val, arena);
        }
      }
    }

    if (next_line == NULL)
      return (char *)(cur + strlen(cur));
    cur = next_line + 1;
  }
  return (char *)cur;
}

static char *resolve_config_subheader(HttpConfig *config, const char *header,
                                      const char *content, Arena *arena) {
  kvAssign assign_func = NULL;
  if (strcmp(header, "[server]") == 0)
    assign_func = resolve_server_key_value_pair;
  return resolve_config_subsection(config, content, arena, assign_func);
}

static void resolve_config_file(HttpConfig *config, const char *content,
                                Arena *arena) {
  const char *cur = content;

  while (*cur != '\0') {
    while (*cur != '\0' && isspace((unsigned char)*cur))
      cur++;
    if (*cur == '\0')
      break;

    if (*cur == '[') {
      const char *end_line = strchr(cur, '\n');
      if (end_line == NULL)
        end_line = cur + strlen(cur);

      const char *close_bracket = strchr(cur, ']');
      if (close_bracket != NULL && close_bracket < end_line) {
        size_t header_len = (size_t)(close_bracket - cur + 1);
        char *header = arena_alloc(arena, header_len + 1);
        if (header == NULL)
          return;

        memcpy(header, cur, header_len);
        header[header_len] = '\0';

        const char *sub_content = (*end_line == '\n') ? end_line + 1 : end_line;
        cur = resolve_config_subheader(config, header, sub_content, arena);
        continue;
      }
    }
    const char *next_line = strchr(cur, '\n');
    if (next_line == NULL)
      break;
    cur = next_line + 1;
  }
}

HttpConfig chttp_config_init(int port, const char *pub_dir) {
  return (HttpConfig){.port = (port <= 0) ? 8080 : port,
                      .public_dir = (pub_dir == NULL) ? "." : pub_dir,
                      .log_filepath = NULL,
                      .log_level = LOG_LEVEL_INFO,
                      .thread_count = 4,
                      .worker_arena_size = 8192}; // 8 KB default
}

HttpConfig chttp_config_load(const char *filepath, Arena *arena) {
  HttpConfig config = chttp_config_init(-1, NULL);

  char *abs_path = chttp_resolve_path(filepath, ".", arena);
  if (abs_path == NULL) {
    return config;
  }

  size_t file_len = 0;
  char *content = chttp_read_file(abs_path, arena, &file_len);
  if (content != NULL) {
    resolve_config_file(&config, content, arena);
  }

  return config;
}
