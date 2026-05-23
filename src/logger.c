#define _POSIX_C_SOURCE 200112L
#include "chttp/logger.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TIME_BUF_SIZE 64

static struct {
  pthread_mutex_t mutex;
  FILE *stream;
  LogLevel min_level;
  int is_file;
} g_logger = {.mutex = PTHREAD_MUTEX_INITIALIZER,
              .stream = NULL,
              .min_level = LOG_LEVEL_INFO,
              .is_file = 0};

static const char *level_strings[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};

int chttp_logger_init(const char *log_filepath, LogLevel min_level) {
  g_logger.min_level = min_level;

  if (pthread_mutex_init(&g_logger.mutex, NULL) != 0) {
    return -1;
  }

  if (log_filepath != NULL) {
    g_logger.stream = fopen(log_filepath, "ae");
    if (g_logger.stream == NULL) {
      pthread_mutex_destroy(&g_logger.mutex);
      return -1;
    }
    g_logger.is_file = 1;
  } else {
    g_logger.stream = stdout;
    g_logger.is_file = 0;
  }

  return 0;
}

void chttp_logger_free(void) {
  pthread_mutex_lock(&g_logger.mutex);
  if (g_logger.is_file && g_logger.stream != NULL) {
    fclose(g_logger.stream);
  }
  g_logger.stream = NULL;
  pthread_mutex_unlock(&g_logger.mutex);
  pthread_mutex_destroy(&g_logger.mutex);
}

void chttp_log_write(LogLevel level, const char *file, int line,
                     const char *fmt, ...) {
  if (level < g_logger.min_level || g_logger.stream == NULL) {
    return;
  }

  time_t now = time(NULL);
  struct tm time_info;
  localtime_r(&now, &time_info);

  char time_buf[TIME_BUF_SIZE];
  strftime(time_buf, TIME_BUF_SIZE, "%Y-%m-%d %H:%M:%S", &time_info);

  pthread_mutex_lock(&g_logger.mutex);
  fprintf(g_logger.stream, "%s %s [%s:%d]: ", time_buf, level_strings[level],
          file, line);

  va_list args;
  va_start(args, fmt);
  vfprintf(g_logger.stream, fmt, args);
  va_end(args);

  fprintf(g_logger.stream, "\n");
  fflush(g_logger.stream);
  pthread_mutex_unlock(&g_logger.mutex);
}

void chttp_log_http(const char *client_ip, const char *request_line,
                    int status_code, size_t response_size) {
  if (g_logger.stream == NULL)
    return;

  time_t now = time(NULL);
  struct tm time_info;
  localtime_r(&now, &time_info);
  char time_buf[TIME_BUF_SIZE];
  strftime(time_buf, TIME_BUF_SIZE, "%d/%b/%Y:%H:%M:%S %z", &time_info);

  pthread_mutex_lock(&g_logger.mutex);
  fprintf(g_logger.stream, "%s - - [%s] \"%s\" %d %zu\n",
          client_ip ? client_ip : "-", time_buf,
          request_line ? request_line : "-", status_code, response_size);
  fflush(g_logger.stream);
  pthread_mutex_unlock(&g_logger.mutex);
}