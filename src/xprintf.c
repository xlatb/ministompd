#include "bytestring.h"
#include "bytestring_printf.h"
#include "printbuf.h"
#include "xprintf.h"

ssize_t xprintf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  ssize_t ret = xvdprintf(STDOUT_FILENO, fmt, args);

  va_end(args);

  return ret;
}

ssize_t xdprintf(int fd, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  ssize_t ret = xvdprintf(fd, fmt, args);

  va_end(args);

  return ret;
}

ssize_t xvdprintf(int fd, const char *fmt, va_list args)
{
  bytestring *bs = printbuf_acquire();

  bytestring_vprintf(bs, fmt, args);
  ssize_t ret = bytestring_write_fd(bs, fd);

  printbuf_release(bs);

  return ret;
}
