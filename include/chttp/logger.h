#ifndef CHTTP_LOGGER_H
#define CHTTP_LOGGER_H

#include <stddef.h>

typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR
} LogLevel;

// initialise the logger state. log_filepath of NULL, defaults to stdout.
int chttp_logger_init(const char *log_filepath, LogLevel min_level);
void chttp_logger_free(void);
void chttp_log_write(LogLevel level, const char *file, int line,
                     const char *fmt, ...);
void chttp_log_http(const char *client_ip, const char *request_line,
                    int status_code, size_t response_size);

#define LOG_DEBUG(...)                                                         \
  chttp_log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)                                                          \
  chttp_log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)                                                          \
  chttp_log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
  chttp_log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif