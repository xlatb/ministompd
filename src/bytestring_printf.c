#include <ctype.h>
#include <stdint.h>
#include <stddef.h>  // size_t, ptrdiff_t
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>  // va_list
#include <stdio.h>  // snprintf()
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

enum fmt_spec_length_mod
{
  FMT_LMOD_NONE         = 0x000,
  FMT_LMOD_INVALID      = 0x001,

  FMT_LMOD_CHAR_INT     = 0x010,
  FMT_LMOD_SHORT_INT    = 0x020,
  FMT_LMOD_LONG_INT     = 0x030,
  FMT_LMOD_LONGLONG_INT = 0x040,
  FMT_LMOD_MAX_INT      = 0x050,
  FMT_LMOD_SIZE_INT     = 0x060,
  FMT_LMOD_PTRDIFF_INT  = 0x070,

  FMT_LMOD_LONG_DOUBLE  = 0x100,
};

const char *fmt_spec_flag_chars = "0+-# ";  // NOTE: Same order as in bit flags above

const char *fmt_spec_conversion_chars = "abAcdeEfFgGouxXps";  // No special order required

struct fmt_spec
{
  uint32_t                 flags;
  int                      width;
  int                      precision;
  enum fmt_spec_length_mod length_mod;
  char                     conversion;
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

  // Length modifier
  enum fmt_spec_length_mod lmod = FMT_LMOD_NONE;
  while (*f)
  {
    if (*f == 'h')
      lmod = (lmod == FMT_LMOD_NONE) ? FMT_LMOD_SHORT_INT : ((lmod == FMT_LMOD_SHORT_INT) ? FMT_LMOD_CHAR_INT : FMT_LMOD_INVALID);
    else if (*f == 'l')
      lmod = (lmod == FMT_LMOD_NONE) ? FMT_LMOD_LONG_INT : ((lmod == FMT_LMOD_LONG_INT) ? FMT_LMOD_LONGLONG_INT : FMT_LMOD_INVALID);
    else if (*f == 'j')
      lmod = (lmod == FMT_LMOD_NONE) ? FMT_LMOD_MAX_INT : FMT_LMOD_INVALID;
    else if (*f == 'z')
      lmod = (lmod == FMT_LMOD_NONE) ? FMT_LMOD_SIZE_INT : FMT_LMOD_INVALID;
    else if (*f == 't')
      lmod = (lmod == FMT_LMOD_NONE) ? FMT_LMOD_PTRDIFF_INT : FMT_LMOD_INVALID;
    else if (*f == 'L')
      lmod = (lmod == FMT_LMOD_NONE) ? FMT_LMOD_LONG_DOUBLE : FMT_LMOD_INVALID;
    else
      break;

    if (lmod == FMT_LMOD_INVALID)
      return false;

    f++;
  }

  fs->length_mod = lmod;

  // Conversion character
  char *conv = strchr(fmt_spec_conversion_chars, *f);
  if (conv == NULL)
    return false;

  fs->conversion = *conv;

  if (length)
    *length = f - fmt + 1;

  return true;
}

static const char *generate_fmt_spec(const struct fmt_spec *fs)
{
  static bytestring *fmt = NULL;
  if (fmt == NULL)
  {
    fmt = bytestring_new(64);  // Reasonable size for format specifiers
    bytestring_append_byte(fmt, '%');
  }

  bytestring_truncate(fmt, 1);

  if (fs->flags & FMT_FLAG_ZERO)
    bytestring_append_byte(fmt, '0');

  if (fs->flags & FMT_FLAG_PLUS)
    bytestring_append_byte(fmt, '+');

  if (fs->flags & FMT_FLAG_MINUS)
    bytestring_append_byte(fmt, '-');

  if (fs->flags & FMT_FLAG_OCTOTHORPE)
    bytestring_append_byte(fmt, '#');

  if (fs->flags & FMT_FLAG_SPACE)
    bytestring_append_byte(fmt, ' ');

  if (fs->flags & FMT_FLAG_HAS_WIDTH)
    bytestring_append_int(fmt, fs->width);

  if (fs->flags & FMT_FLAG_HAS_PRECISION)
  {
    bytestring_append_byte(fmt, '.');
    bytestring_append_int(fmt, fs->precision);
  }

  switch (fs->length_mod)
  {
    case FMT_LMOD_NONE:
    case FMT_LMOD_INVALID:
      break;
    case FMT_LMOD_CHAR_INT:
      bytestring_append_byte(fmt, 'h');
      // Fallthrough
    case FMT_LMOD_SHORT_INT:
      bytestring_append_byte(fmt, 'h');
      break;
    case FMT_LMOD_LONGLONG_INT:
      bytestring_append_byte(fmt, 'l');
      // Fallthrough
    case FMT_LMOD_LONG_INT:
      bytestring_append_byte(fmt, 'l');
      break;
    case FMT_LMOD_MAX_INT:
      bytestring_append_byte(fmt, 'j');
      break;
    case FMT_LMOD_SIZE_INT:
      bytestring_append_byte(fmt, 'z');
      break;
    case FMT_LMOD_PTRDIFF_INT:
      bytestring_append_byte(fmt, 't');
      break;
    case FMT_LMOD_LONG_DOUBLE:
      bytestring_append_byte(fmt, 'L');
      break;
  }

  bytestring_append_byte(fmt, fs->conversion);
  bytestring_append_byte(fmt, 0);

  return (const char *) bytestring_get_bytes(fmt);
}

#define FMT_SPEC(conv, type) \
static void bytestring_print_fmt_spec_ ## conv(bytestring *bs, const struct fmt_spec *fs, type arg) \
{ \
  const char *fmt = generate_fmt_spec(fs); \
  int avail = bs->size - bs->length; \
  int count = snprintf((char *) (bs->data + bs->length), avail, fmt, arg); \
  if (count < 0) \
    abort(); \
  else if (count < avail) \
  { \
    bs->length += count; \
    return; \
  } \
  bytestring_ensure_size(bs, bs->size + count + 1); \
  count = snprintf((char *) (bs->data + bs->length), avail, fmt, arg); \
  bs->length += count; \
  return; \
}

FMT_SPEC(cdoux, int);
FMT_SPEC(aefg, double);
FMT_SPEC(p, void *);
FMT_SPEC(s, const char *);

FMT_SPEC(l_doux, long);
FMT_SPEC(ll_doux, long long);

FMT_SPEC(z_doux, size_t);
FMT_SPEC(j_doux, intmax_t);
FMT_SPEC(t_doux, ptrdiff_t);

FMT_SPEC(L_aefg, long double);

// Nonstandard %b for printing 'bytestring *'.
static void bytestring_print_fmt_spec_b(bytestring *bs1, const struct fmt_spec *fs, const bytestring *bs2)
{
  // TODO: Support flags and width/precision?
  bytestring_append_bytestring(bs1, bs2);
}

static void bytestring_print_fmt_spec(bytestring *bs, const struct fmt_spec *fs, va_list args)
{
  switch (fs->length_mod)
  {
  case FMT_LMOD_NONE:
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
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_cdoux(bs, fs, va_arg(args, int));
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
      abort();
    }
    break;
  case FMT_LMOD_CHAR_INT:   // 'char' and 'short' are promoted to 'int' by
  case FMT_LMOD_SHORT_INT:  //  varargs.
    switch (fs->conversion)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_cdoux(bs, fs, va_arg(args, int));
      break;
    default:
      abort();
    }
    break;
  case FMT_LMOD_LONG_INT:
    switch (fs->conversion)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_l_doux(bs, fs, va_arg(args, long));
      break;
    default:
      abort();
    }
    break;
  case FMT_LMOD_LONGLONG_INT:
    switch (fs->conversion)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_ll_doux(bs, fs, va_arg(args, long long));
      break;
    default:
      abort();
    }
    break;
  case FMT_LMOD_MAX_INT:
    switch (fs->conversion)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_j_doux(bs, fs, va_arg(args, intmax_t));
      break;
    default:
      abort();
    }
    break;
  case FMT_LMOD_SIZE_INT:
    switch (fs->conversion)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_z_doux(bs, fs, va_arg(args, size_t));
      break;
    default:
      abort();
    }
    break;
  case FMT_LMOD_PTRDIFF_INT:
    switch (fs->conversion)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      bytestring_print_fmt_spec_t_doux(bs, fs, va_arg(args, ptrdiff_t));
      break;
    default:
      abort();
    }
    break;
  case FMT_LMOD_LONG_DOUBLE:
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
      bytestring_print_fmt_spec_L_aefg(bs, fs, va_arg(args, long double));
      break;
    default:
      abort();
    }
    break;
  default:
    abort();
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
          break;  // It's not safe to continue with following format specifiers
        }
      }
    }
  }

  return bs;
}
