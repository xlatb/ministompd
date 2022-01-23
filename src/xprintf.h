#ifndef MINISTOMPD_XPRINTF_H
#define MINISTOMPD_XPRINTF_H

ssize_t xprintf(const char *fmt, ...);
ssize_t xdprintf(int fd, const char *fmt, ...);
ssize_t xvdprintf(int fd, const char *fmt, va_list args);

#endif
