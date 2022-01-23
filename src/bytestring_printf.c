#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>  // memcpy()
#include <stdarg.h>  // va_list
#include <stdio.h>  // vsnprintf()
#include "alloc.h"

#include "bytestring.h"
#include "bytestring_printf.h"

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

const char *fmt_spec_conversion_chars = "abAcdeEfFgGouxXps";  // No special order required

struct fmt_spec
{
  uint32_t flags;
  int      width;
  int      precision;
  char     conversion;
};

// Given a string that is assumed to have been preceeded by '%', parse as a
//  format specifier.
// If length is provided, we write the original length to it.
static bool parse_fmt_spec(const char *fmt, struct fmt_spec *fs, int *length)
{
  const char *f = fmt;

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

  char *conv = strchr(fmt_spec_conversion_chars, *f);
  if (conv == NULL)
    return false;

  fs->conversion = *conv;

  if (length)
    *length = f - fmt + 1;

  return true;
}

#define FMT_SPEC(conv, type) \
static void bytestring_print_fmt_spec_ ## conv(bytestring *bs, const struct fmt_spec *fs, type arg) \
{ \
  static char fmt[6] = "%"; \
  uint32_t width_prec = fs->flags & (FMT_FLAG_HAS_WIDTH | FMT_FLAG_HAS_PRECISION); \
  int avail = bs->size - bs->length; \
  int length; \
  if (width_prec == 0) \
  { \
    fmt[1] = fs->conversion; \
    fmt[2] = '\0'; \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_WIDTH) \
  { \
    fmt[1] = '*'; \
    fmt[2] = fs->conversion; \
    fmt[3] = '\0'; \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, fs->width, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_PRECISION) \
  { \
    fmt[1] = '.'; \
    fmt[2] = '*'; \
    fmt[3] = fs->conversion; \
    fmt[4] = '\0'; \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, fs->precision, arg); \
  } \
  else if (width_prec == (FMT_FLAG_HAS_WIDTH | FMT_FLAG_HAS_PRECISION)) \
  { \
    fmt[1] = '*'; \
    fmt[2] = '.'; \
    fmt[3] = '*'; \
    fmt[4] = fs->conversion; \
    fmt[5] = '\0'; \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, fs->width, fs->precision, arg); \
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
    bs->length += length; \
    return; \
  } \
  bytestring_ensure_size(bs, bs->size + length); \
  if (width_prec == 0) \
  { \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_WIDTH) \
  { \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, fs->width, arg); \
  } \
  else if (width_prec == FMT_FLAG_HAS_PRECISION) \
  { \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, fs->precision, arg); \
  } \
  else if (width_prec == (FMT_FLAG_HAS_WIDTH | FMT_FLAG_HAS_PRECISION)) \
  { \
    length = snprintf((char *) (bs->data + bs->length), avail, fmt, fs->width, fs->precision, arg); \
  } \
  bs->length += length; \
  return; \
}

FMT_SPEC(cd, int);
FMT_SPEC(oux, unsigned int);
FMT_SPEC(aefg, double);
FMT_SPEC(p, void *);
FMT_SPEC(s, const char *);

// Nonstandard %b for printing 'bytestring *'.
static void bytestring_print_fmt_spec_b(bytestring *bs1, const struct fmt_spec *fs, const bytestring *bs2)
{
  // TODO: Support flags and width/precision?
  bytestring_append_bytestring(bs1, bs2);
}

static void bytestring_print_fmt_spec(bytestring *bs, const struct fmt_spec *fs, va_list args)
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
    bytestring_print_fmt_spec_aefg(bs, fs, va_arg(args, double));
    break;
  case 'c':
  case 'd':
    bytestring_print_fmt_spec_cd(bs, fs, va_arg(args, int));
    break;
  case 'o':
  case 'u':
  case 'x':
  case 'X':
    bytestring_print_fmt_spec_oux(bs, fs, va_arg(args, unsigned int));
    break;
  case 'p':
    bytestring_print_fmt_spec_p(bs, fs, va_arg(args, void *));
    break;
  case 's':
    bytestring_print_fmt_spec_s(bs, fs, va_arg(args, const char *));
    break;
  case 'b':
    bytestring_print_fmt_spec_b(bs, fs, va_arg(args, const bytestring *));
    break;
  default:
    abort();  // Unknown conversion
  }
}

bytestring *bytestring_vprintf_internal(bytestring *bs, const char *fmt, va_list args)
{
  const char *f = fmt;
  while (*f)
  {
    if (*f != '%')
    {
      // Find run of literal text
      const char *start = f;
      while (*f && (*f != '%'))
        f++;

      // Add the run
      size_t len = f - start;
      bytestring_append_bytes(bs, (const uint8_t *) start, len);
    }
    else
    {
      f++;
      if (*f == '%')
      {
        // Double percent
        bytestring_append_byte(bs, '%');
      }
      else
      {
        struct fmt_spec fs;
        int len;
        if (parse_fmt_spec(f, &fs, &len))
        {
          // Handle format specifier
          f += len;
          bytestring_print_fmt_spec(bs, &fs, args);
        }
        else
        {
          // Add error
          const char *msg = "<Bad format specifier>";
          bytestring_append_bytes(bs, (const uint8_t *) msg, strlen(msg));
        }
      }
    }
  }

  return bs;
}
