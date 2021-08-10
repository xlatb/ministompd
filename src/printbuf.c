#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>  // memcpy()
#include <stdarg.h>  // va_list
#include <stdio.h>  // vsnprintf()
#include "printbuf.h"
#include "alloc.h"

static printbuf *shared_printbuf = NULL;

enum fmt_spec_flags
{
  FMT_FLAG_HAS_WIDTH      = 0x1,
  FMT_FLAG_HAS_PRECISION  = 0x2,
  FMT_FLAG_GRAB_WIDTH     = 0x4,  // TODO
  FMT_FLAG_GRAB_PRECISION = 0x8,  // TODO

  FMT_FLAG_ZERO           = 0x010,
  FMT_FLAG_PLUS           = 0x020,
  FMT_FLAG_MINUS          = 0x040,
  FMT_FLAG_OCTOTHORPE     = 0x080,
  FMT_FLAG_SPACE          = 0x100,
};

const char *fmt_spec_flag_chars = "0+-# ";  // NOTE: Same order as in bit flags above

struct fmt_spec
{
  uint32_t flags;
  int      width;
  int      precision;
  char     conversion;
};

// Given an integer, returns the number of characters needed to represent
//  it as a decimal number.
static int decimal_char_length(int num)
{
  int count = 1;

  if (num < 0)
  {
    count++;  // One extra place for minus sign

    while (num <= -10)
    {
      num /= 10;
      count++;
    }
  }
  else
  {
    while (num >= 10)
    {
      num /= 10;
      count++;
    }
  }

  return count;
}

// Given a string that is assumed to have been preceeded by '%', parse as a
//  format specifier.
// If length is provided, we write the original length to it.
static bool parse_fmt_spec(char *fmt, struct fmt_spec *fs, int *length)
{
  char *f = fmt;

  fs->flags = 0;

  // Flags
  char *flag;
  while ((*f) && (flag = strchr(fmt_spec_flag_chars, *f)))
  {
    fs->flags |= FMT_FLAG_ZERO << (flag - fmt_spec_flag_chars);
    f++;
  }

  // Width
  if (isdigit(*f))
  {
    char *end;
    fs->width = strtol(f, &end, 10);
    f = end;
    fs->flags |= FMT_FLAG_HAS_WIDTH;
  }

  // Precision
  if (*f == '.')
  {
    // NOTE: C11 says "if only the period is specified, the precision is taken
    //  as zero." Conveniently, the strtol() will return 0 if there are no
    //  digits found.
    char *end;
    fs->precision = strtol(f + 1, &end, 10);
    f = end;
    fs->flags |= FMT_FLAG_HAS_PRECISION;
  }

  char *conv = strchr("aAcdeEfFgGouxXps", *f);
  if (conv == NULL)
    return false;

  fs->conversion = *conv;

  if (length)
    *length = f - fmt + 1;

  return true;
}

// Reconstructs a format string from a struct fmt_spec.
static const char *generate_fmt_spec(struct fmt_spec *fs)
{
  static printbuf *fmt = NULL;
  if (fmt == NULL)
  {
    fmt = printbuf_new(64);  // Reasonable size for format specifiers
    printbuf_append_chars(fmt, "%", 1);
  }

  printbuf_truncate(fmt, 1);

  if (fs->flags & FMT_FLAG_HAS_WIDTH)
    printbuf_append_int(fmt, fs->width);

  if (fs->flags & FMT_FLAG_HAS_PRECISION)
  {
    printbuf_append_chars(fmt, ".", 1);
    printbuf_append_int(fmt, fs->precision);
  }

  printbuf_append_chars(fmt, &fs->conversion, 1);

  return fmt->buf;
}

printbuf *printbuf_new(int size)
{
  struct printbuf *pb = xmalloc(sizeof(struct printbuf));
  pb->buf    = xmalloc(size);
  pb->size   = size;
  pb->length = 0;

  return pb;
}

static printbuf *printbuf_new_shared(void)
{
  if (shared_printbuf == NULL)
    shared_printbuf = printbuf_new(2048);

  shared_printbuf->length = 0;

  return shared_printbuf;
}

static void printbuf_ensure_size(printbuf *pb, size_t requested)
{
  if (requested <= pb->size)
    return;  // Already large enough

  size_t newsize = pb->size;
  while (newsize < requested)
    newsize <<= 1;

  pb->buf  = xrealloc(pb->buf, newsize);
  pb->size = newsize;
}

printbuf *printbuf_append_chars(printbuf *pb, const char *src, size_t len)
{
  if (pb == NULL)
    pb = printbuf_new_shared();

  printbuf_ensure_size(pb, pb->length + len);
  memcpy(pb->buf + pb->length, src, len);
  pb->length += len;

  return pb;
}

int printbuf_truncate(printbuf *pb, int len)
{
  if (len > pb->size)
    len = pb->size;

  pb->length = len;

  return len;
}

printbuf *printbuf_append_int(printbuf *pb, int i)
{
  if (pb == NULL)
    pb = printbuf_new_shared();

  bool negative = i < 0;
  int length = decimal_char_length(i);

  printbuf_ensure_size(pb, pb->length + length);

  char *pos = pb->buf + pb->length + length - 1;
  while (i)
  {
    *pos-- = '0' + abs(i % 10);
    i /= 10;
  }

  if (negative)
    *pos = '-';

  pb->length += length;

  return pb;
}

#define FMT_SPEC(conv, type) \
static void printbuf_print_fmt_spec_ ## conv(printbuf *pb, const struct fmt_spec *fs, type arg) \
{ \
  static char fmt[6] = "%"; \
  uint32_t width_prec = fs->flags & (FMT_FLAG_HAS_WIDTH | FMT_FLAG_HAS_PRECISION); \
  int avail = pb->size - pb->length; \
  int length; \
  if (width_prec == 0) \
  { \
    fmt[1] = fs->conversion; \
    fmt[2] = '\0'; \
    length = snprintf(pb->buf + pb->length, avail, fmt, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_WIDTH) \
  { \
    fmt[1] = '*'; \
    fmt[2] = fs->conversion; \
    fmt[3] = '\0'; \
    length = snprintf(pb->buf + pb->length, avail, fmt, fs->width, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_PRECISION) \
  { \
    fmt[1] = '.'; \
    fmt[2] = '*'; \
    fmt[3] = fs->conversion; \
    fmt[4] = '\0'; \
    length = snprintf(pb->buf + pb->length, avail, fmt, fs->precision, arg); \
  } \
  else if (width_prec == (FMT_FLAG_HAS_WIDTH | FMT_FLAG_HAS_PRECISION)) \
  { \
    fmt[1] = '*'; \
    fmt[2] = '.'; \
    fmt[3] = '*'; \
    fmt[4] = fs->conversion; \
    fmt[5] = '\0'; \
    length = snprintf(pb->buf + pb->length, avail, fmt, fs->width, fs->precision, arg); \
  } \
  else \
  { \
    abort(); \
  } \
  if (length < 0) \
  { \
    return; \
  } \
  else if (length <= avail) \
  { \
    pb->length += length; \
    return; \
  } \
  printbuf_ensure_size(pb, pb->size + length); \
  if (width_prec == 0) \
  { \
    length = snprintf(pb->buf + pb->length, avail, fmt, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_WIDTH) \
  { \
    length = snprintf(pb->buf + pb->length, avail, fmt, fs->width, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_PRECISION) \
  { \
    length = snprintf(pb->buf + pb->length, avail, fmt, fs->precision, arg); \
  } \
  else if (width_prec == (FMT_FLAG_HAS_WIDTH | FMT_FLAG_HAS_PRECISION)) \
  { \
    length = snprintf(pb->buf + pb->length, avail, fmt, fs->width, fs->precision, arg); \
  } \
  pb->length += length; \
  return; \
}

FMT_SPEC(cd, int);
FMT_SPEC(oux, unsigned int);
FMT_SPEC(aefg, double);
FMT_SPEC(p, void *);
FMT_SPEC(s, const char *);

static void printbuf_print_fmt_spec(printbuf *pb, const struct fmt_spec *fs, va_list args)
{
  switch (fs->conversion)
  {
  case 'a':
  case 'A':
  case 'e':
  case 'E':
  case 'f':
  case 'F':
  case 'g':
  case 'G':
    printbuf_print_fmt_spec_aefg(pb, fs, va_arg(args, double));
    break;
  case 'c':
  case 'd':
    printbuf_print_fmt_spec_cd(pb, fs, va_arg(args, int));
    break;
  case 'o':
  case 'u':
  case 'x':
  case 'X':
    printbuf_print_fmt_spec_oux(pb, fs, va_arg(args, unsigned int));
    break;
  case 'p':
    printbuf_print_fmt_spec_p(pb, fs, va_arg(args, void *));
    break;
  case 's':
    printbuf_print_fmt_spec_s(pb, fs, va_arg(args, const char *));
    break;
  default:
    abort();  // Unknown conversion
  }
}

printbuf *printbuf_printf(printbuf *pb, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  printbuf *ret = printbuf_vprintf(pb, fmt, args);

  va_end(args);

  return ret;
}

printbuf *printbuf_vprintf(printbuf *pb, char *fmt, va_list args)
{
  if (pb == NULL)
    pb = printbuf_new_shared();

  char *f = fmt;
  while (*f)
  {
    if (*f != '%')
    {
      // Find run of literal text
      char *start = f;
      while (*f && (*f != '%'))
        f++;

      // Add the run
      size_t len = f - start;
      printbuf_ensure_size(pb, pb->length + len);
      memcpy(pb->buf + pb->length, start, len);
      pb->length += len;
    }
    else
    {
      f++;
      if (*f == '%')
      {
        // Double percent
        printbuf_ensure_size(pb, pb->length + 1);
        pb->buf[pb->length++] = '%';
      }
      else
      {
        struct fmt_spec fs;
        int len;
        if (parse_fmt_spec(f, &fs, &len))
        {
          // Handle format specifier
          f += len;
          printbuf_print_fmt_spec(pb, &fs, args);
        }
        else
        {
          // Add error
          const char *msg = "<Bad format specifier>";
          printbuf_ensure_size(pb, pb->length + strlen(msg));
          memcpy(pb->buf + pb->length, msg, strlen(msg));
        }
      }
    }
  }

  return pb;
}
