#include <stdbool.h>
#include <stdarg.h>

#ifndef MINISTOMPD_LOG_H
#define MINISTOMPD_LOG_H

typedef enum
{
  LOG_LEVEL_NONE,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_INFO,
  LOG_LEVEL_VERBOSE,
  LOG_LEVEL_DEBUG
} log_level;

void log_set_level(log_level lvl);
bool log_check_level(log_level lvl);
bool log_printf(log_level lvl, const char *fmt, ...);
bool log_perror(log_level lvl, const char *msg);

#endif
