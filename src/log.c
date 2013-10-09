#define _POSIX_C_SOURCE 200809L  // dprintf()

#include <stdio.h>
#include <errno.h>
#include <time.h>    // strftime()
#include <string.h>  // strerror()
#include <unistd.h>  // STDERR_FILENO
#include "log.h"

static log_level level = LOG_LEVEL_INFO;
static int       fd    = STDERR_FILENO;

void log_set_level(log_level lvl)
{
  level = lvl;
}

bool log_check_level(log_level lvl)
{
  return (lvl <= level);
}

bool log_printf(log_level lvl, const char *fmt, ...)
{
  va_list args;

  if (lvl > level)
    return false;

//  // Prepend timestamp
//  time_t t;
//  char timestamp[20];
//  struct tm *tm;
//  t = time(NULL);
//  tm = localtime(&t);
//  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
//  strcpy(timestamp, "2013-10-08 22:56:55");
//  dprintf(fd, "[%s] ", timestamp);

  va_start(args, fmt);
  vdprintf(fd, fmt, args);
  va_end(args);

  return true;
}

bool log_perror(log_level lvl, const char *msg)
{
  int err = errno;

  return log_printf(lvl, "%s: %s\n", msg, strerror(err));
}
