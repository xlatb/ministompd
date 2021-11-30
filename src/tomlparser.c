#include "ministompd.h"
#include "tomlparser.h"
#include "unicode.h"
#include "list.h"
#include "hash.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>  // INFINITY, NAN, pow()

// Frees a 'keypath', which is just a list of bytestrings.
static void toml_keypath_free(list *keypath)
{
  if (keypath == NULL)
    return;

  bytestring *bs;
  while ((bs = list_pop(keypath)))
    bytestring_free(bs);

  list_free(keypath);
  return;
}

tomlparser *tomlparser_new(void)
{
  tomlparser *tp = xmalloc(sizeof(tomlparser));

  tp->file    = NULL;
  tp->line    = 0;
  tp->peekbuf = buffer_new(64);

  tp->errors  = NULL;

  tp->root    = tomlvalue_new_table();
  tp->current = tp->root;

  return tp;
}

static void tomlparser_clear_errors(tomlparser *tp)
{
  if (!tp->errors)
    return;

  void *err;
  while ((err = list_pop(tp->errors)))
    xfree(err);

  return;
}

// Returns the next error, if there is one. Caller should xfree() if not NULL.
struct toml_error *tomlparser_pull_error(tomlparser *tp)
{
  if (!tp->errors)
    return NULL;

  struct toml_error *err = list_shift(tp->errors);
  return err;
}

static void tomlparser_record_error(tomlparser *tp, const char *message)
{
  if (!tp->errors)
    tp->errors = list_new(3);

  struct toml_error *err = xmalloc(sizeof(struct toml_error));
  err->message = message;
  err->line    = tp->line;

  list_push(tp->errors, err);

  return;
}

void tomlparser_set_file(tomlparser *tp, FILE *f)
{
  tp->file = f;
  tp->line = 1;

  tomlparser_clear_errors(tp);
}

// Tries to ensure the peekbuf has at least the given number of bytes in it.
// Returns true if the requested number of bytes could be satisfied.
static bool tomlparser_ensure_peekbuf_length(tomlparser *tp, int count)
{
  int len = buffer_get_length(tp->peekbuf);
  if (len >= count)
    return true;  // Already enough bytes

  int needed = count - len;
  uint8_t readbuf[needed];
  int ret = fread(readbuf, 1, needed, tp->file);
  buffer_input_bytes(tp->peekbuf, readbuf, ret);

  return (ret >= needed) ? true : false;
}

// Returns the next char from the buffer or EOF.
// The character is not consumed.
static int tomlparser_peek_char(tomlparser *tp)
{
  if (!tomlparser_ensure_peekbuf_length(tp, 1))
    return EOF;

  return buffer_get_byte(tp->peekbuf, 0);
}

// If the start of the peek buffer entirely matches the given string, returns
//  the string length. Otherwise, returns zero.
static int tomlparser_peek_match(tomlparser *tp, const char *str)
{
  int len = strlen(str);

  if (!tomlparser_ensure_peekbuf_length(tp, len))
    return false;  // Not enough characters available

  for (int i = 0; i < len; i++)
  {
    if (buffer_get_byte(tp->peekbuf, i) != (uint8_t) str[i])
      return 0;
  }

  return len;
}

// Counts the number of upcoming characters, starting from the given position,
//  which are within the given set.
static int tomlparser_peek_count_set(tomlparser *tp, int pos, const char *set)
{
  int reqlen = 32;
  tomlparser_ensure_peekbuf_length(tp, reqlen);
  int buflen = buffer_get_length(tp->peekbuf);

  int setlen = strlen(set);

  int count = 0;
  for (int p = pos; p < buflen; p++)
  {
    char c = buffer_get_byte(tp->peekbuf, p);
    if (!memchr(set, c, setlen))
      break;

    count++;
    if (p == reqlen)
    {
      reqlen *= 2;
      tomlparser_ensure_peekbuf_length(tp, reqlen);
      buflen = buffer_get_length(tp->peekbuf);
    }
  }

  return count;
}

// Returns the next char from the buffer or EOF.
// The character is consumed.
static int tomlparser_get_char(tomlparser *tp)
{
  if (!tomlparser_ensure_peekbuf_length(tp, 1))
    return EOF;

  int c = buffer_get_byte(tp->peekbuf, 0);
  buffer_consume(tp->peekbuf, 1);
  return c;
}

static bool tomlparser_consume_match(tomlparser *tp, const char *str)
{
  int len;
  if ((len = tomlparser_peek_match(tp, str)) == 0)
    return false;

  buffer_consume(tp->peekbuf, len);
  return true;
}

static void tomlparser_consume(tomlparser *tp, int count)
{
  buffer_consume(tp->peekbuf, count);
}

static tomlvalue *tomlparser_walk(tomlparser *tp, tomlvalue *start, list *keypath, int depth)
{
  tomlvalue *node = start;

  int len = list_get_length(keypath);
  if (depth < 1)
    depth = len + depth;
  else
    depth = (depth < len) ? depth : len;

  for (int i = 0; i < depth; i++)
  {
    bytestring *key = list_get_item(keypath, i);

    if (node->type != TOML_TYPE_TABLE)
    {
      tomlparser_record_error(tp, "Attempt to use non-table as table");
      return NULL;
    }

    tomlvalue *subvalue = hash_get(node->u.tableval, key);
    if (subvalue == NULL)
    {
      // Auto-vivify table
      subvalue = tomlvalue_new_table();
      hash_add(node->u.tableval, key, subvalue);
    }

    node = subvalue;
  }

  return node;
}

// Skip over tab (0x09) and space (0x20).
static bool tomlparser_skip_whitespace(tomlparser *tp)
{
  int c;

  while ((c = tomlparser_peek_char(tp)) != EOF)
  {
    if ((c != '\x09') && (c != '\x20'))
      return true;  // No more whitespace

    tomlparser_consume(tp, 1);
  }

  return false;  // EOF
}

static bool tomlparser_skip_newline(tomlparser *tp)
{
  int c = tomlparser_get_char(tp);
  if (c == '\x0A')  // LF
  {
    tp->line++;
    return true;
  }
  else if (c == '\x0D')  // CR
  {
    // Also skip following LF if present
    tp->line++;
    c = tomlparser_peek_char(tp);
    if (c == '\x0A')  // CRLF
      tomlparser_consume(tp, 1);
    return true;
  }

  abort();  // Expected newline
}

// Skip until LF (0x0A) or CRLF (0x0D 0x0A).
static bool tomlparser_skip_line(tomlparser *tp)
{
  int c;

  while ((c = tomlparser_peek_char(tp)) != EOF)
  {
    if ((c == '\x0A') || (c == '\x0D'))
    {
      tomlparser_skip_newline(tp);
      return true;
    }

    tomlparser_consume(tp, 1);
  }

  return false;  // EOF
}

// Expects end of line, optionally with a trailing comment
static bool tomlparser_end_line(tomlparser *tp)
{
  tomlparser_skip_whitespace(tp);

  int c = tomlparser_peek_char(tp);
  if (c == EOF)
    return true;  // EOF
  else if (c == '#')
  {
    tomlparser_skip_line(tp);
    return true;
  }
  else if ((c == '\x0A') || (c == '\x0D'))
  {
    tomlparser_skip_newline(tp);
    return true;
  }

  tomlparser_record_error(tp, "Expected comment or EOL");
  return false;
}


// TODO: Leverage tomlparser_get_digit()?
static bool tomlparser_parse_hex_number(tomlparser *tp, int digits, uint32_t *valueptr)
{
  uint32_t value = 0;
  for (int d = 0; d < digits; d++)
  {
    int c = tomlparser_get_char(tp);

    int v;
    if ((c >= '0') && (c <= '9'))
      v = c - '0';
    else if ((c >= 'a') && (c <= 'f'))
      v = (c - 'a') + 10;
    else if ((c >= 'A') && (c <= 'F'))
      v = (c - 'A') + 10;
    else
    {
      tomlparser_record_error(tp, "Illegal character in hex digit string");
      return false;
    }

    value = (value << 4) | v;
  }

  *valueptr = value;
  return true;
}

// NOTE: Assumes the leading backslash has already been consumed.
static bool tomlparser_parse_basic_string_escape(tomlparser *tp, bytestring *bs)
{
  int c = tomlparser_get_char(tp);
  if (c == 'b')
    bytestring_append_byte(bs, '\b');
  else if (c == 't')
    bytestring_append_byte(bs, '\t');
  else if (c == 'n')
    bytestring_append_byte(bs, '\n');
  else if (c == 'f')
    bytestring_append_byte(bs, '\f');
  else if (c == 'r')
    bytestring_append_byte(bs, '\r');
  else if (c == '"')
    bytestring_append_byte(bs, '"');
  else if (c == '\\')
    bytestring_append_byte(bs, '\\');
  else if ((c == 'u') || (c == 'U'))
  {
    uint32_t ucs4;
    if (!tomlparser_parse_hex_number(tp, (c == 'u') ? 4 : 8, &ucs4))
      return false;

    uint8_t utf8[4];
    size_t len = unicode_ucs4_to_utf8(ucs4, &utf8);
    if (len == 0)
    {
      tomlparser_record_error(tp, "Invalid codepoint");
      return false;
    }

    bytestring_append_bytes(bs, utf8, len);
  }
  else
  {
    tomlparser_record_error(tp, "Unknown basic string backslash escape");
    return false;
  }

  return true;
}

// Parses a "basic" string enclosed in double quotes.
static bool tomlparser_parse_basic_string(tomlparser *tp, bytestring *bs)
{
  int c = tomlparser_get_char(tp);
  if (c != '"')
    abort();

  while (1)
  {
    c = tomlparser_get_char(tp);
    if (c == EOF)
    {
      tomlparser_record_error(tp, "Unexpected EOF in basic string");
      return false;
    }
    else if (c == '"')
      break;
    else if ((c <= 0x08) || ((c >= 0x0A) && (c <= 0x1F)) || (c == 0x7F))
    {
      tomlparser_record_error(tp, "Illegal control character in basic string");
      return false;
    }
    else if (c == '\\')
    {
      if (!tomlparser_parse_basic_string_escape(tp, bs))
        return false;
    }
    else
      bytestring_append_byte(bs, c);
  }

  return true;
}

// Parses a multiline string enclosed in runs of three double quotes.
static bool tomlparser_parse_multiline_basic_string(tomlparser *tp, bytestring *bs)
{
  if (!tomlparser_consume_match(tp, "\"\"\""))
    abort();

  // If string starts with a newline, it is discarded
  int c = tomlparser_peek_char(tp);
  if ((c == '\x0A') || (c == '\x0D'))
    tomlparser_skip_newline(tp);

  int quotecount = 0;
  while (1)
  {
    c = tomlparser_peek_char(tp);
    if (c == EOF)
    {
      tomlparser_record_error(tp, "Unexpected EOF in multiline basic string");
      return false;
    }
    else if (c == '"')
    {
      quotecount++;
      tomlparser_consume(tp, 1);
      if (quotecount == 3)
        break;
      continue;
    }

    // If quotecount is non-zero, we had earlier quotes that never became a delimiter
    while (quotecount > 0)
    {
      quotecount--;
      bytestring_append_byte(bs, '"');
    }

    if ((c == '\x0A') || (c == '\x0D'))
    {
      tomlparser_skip_newline(tp);
      bytestring_append_byte(bs, '\n');
    }
    else if ((c <= 0x08) || ((c >= 0x0A) && (c <= 0x1F)) || (c == 0x7F))
    {
      tomlparser_record_error(tp, "Illegal control character in multiline basic string");
      return false;
    }
    else if (c == '\\')
    {
      tomlparser_consume(tp, 1);
      c = tomlparser_peek_char(tp);
      if ((c == '\x0A') || (c == '\x0D'))
        tomlparser_skip_newline(tp);  // Continuation line; newline ignored
      else if (!tomlparser_parse_basic_string_escape(tp, bs))
        return false;
    }
    else
    {
      tomlparser_consume(tp, 1);
      bytestring_append_byte(bs, c);
    }
  }

  return true;
}

// Parses "literal" string surrounded by single quotes.
static bool tomlparser_parse_literal_string(tomlparser *tp, bytestring *bs)
{
  int c = tomlparser_get_char(tp);
  if (c != '\'')
    abort();

  while (1)
  {
    c = tomlparser_get_char(tp);
    if (c == EOF)
    {
      tomlparser_record_error(tp, "Unexpected EOF in literal string");
      return false;
    }
    else if (c == '\'')
      break;
    else if ((c <= 0x08) || ((c >= 0x0A) && (c <= 0x1F)) || (c == 0x7F))
    {
      tomlparser_record_error(tp, "Illegal control character in literal string");
      return false;
    }
    else
      bytestring_append_byte(bs, c);
  }

  return true;
}

// Parses a multiline string enclosed in runs of three single quotes.
static bool tomlparser_parse_multiline_literal_string(tomlparser *tp, bytestring *bs)
{
  if (!tomlparser_consume_match(tp, "'''"))
    abort();

  // If string starts with a newline, it is discarded
  int c = tomlparser_peek_char(tp);
  if ((c == '\x0A') || (c == '\x0D'))
    tomlparser_skip_newline(tp);

  while (1)
  {
    c = tomlparser_peek_char(tp);
    if (c == EOF)
    {
      tomlparser_record_error(tp, "Unexpected EOF in multiline literal string");
      return false;
    }
    else if (c == '\'')
    {
      // NOTE: TOML allows runs of one or two quotes in any position,
      //  including directly preceeding the final delimiter.
      int quotecount = tomlparser_peek_count_set(tp, 0, "'");
      if (quotecount >= 6)
      {
        tomlparser_record_error(tp, "Invalid run of single quotes within multiline literal string");
        return false;
      }
      else if (quotecount >= 3)
      {
        // End of string, possibly with leading extra single quotes
        int litcount = quotecount - 3;
        for (int i = 0; i < litcount; i++)
          bytestring_append_byte(bs, '\'');
        tomlparser_consume(tp, quotecount);
        break;
      }
      else
      {
        // One or two embedded quotes, taken as literal
        for (int i = 0; i < quotecount; i++)
          bytestring_append_byte(bs, '\'');
        tomlparser_consume(tp, quotecount);
        continue;
      }
    }

    if ((c == '\x0A') || (c == '\x0D'))
    {
      tomlparser_skip_newline(tp);
      bytestring_append_byte(bs, '\n');
    }
    else if ((c <= 0x08) || ((c >= 0x0A) && (c <= 0x1F)) || (c == 0x7F))
    {
      tomlparser_record_error(tp, "Illegal control character in multiline basic string");
      return false;
    }
    else
    {
      tomlparser_consume(tp, 1);
      bytestring_append_byte(bs, c);
    }
  }

  return true;
}

static bool tomlparser_parse_string(tomlparser *tp, bytestring *bs)
{
  if (tomlparser_peek_match(tp, "\"\"\""))
    return tomlparser_parse_multiline_basic_string(tp, bs);
  else if (tomlparser_peek_match(tp, "'''"))
    return tomlparser_parse_multiline_literal_string(tp, bs);
  else if (tomlparser_peek_match(tp, "\""))
    return tomlparser_parse_basic_string(tp, bs);
  else if (tomlparser_peek_match(tp, "'"))
    return tomlparser_parse_literal_string(tp, bs);

  tomlparser_record_error(tp, "Expected string");
  return false;
}

// Parses a "bare" (unquoted) key.
// unquoted-key = 1*( ALPHA / DIGIT / %x2D / %x5F ) ; A-Z / a-z / 0-9 / - / _
static bool tomlparser_parse_bare_key(tomlparser *tp, bytestring *bs)
{
  int len = 0;
  int c;

  while ((c = tomlparser_peek_char(tp)) != EOF)
  {
    if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '-') || (c == '_'))
    {
      bytestring_append_byte(bs, c);
      tomlparser_consume(tp, 1);
      len++;
    }
    else
      break;
  }

  return (len > 0);
}

static bool tomlparser_get_digit(tomlparser *tp, int *digitptr, int base)
{
  int c = tomlparser_peek_char(tp);

  int d;
  if ((c >= '0') && (c <= '9'))
    d = c - '0';
  else if ((c >= 'a') && (c <= 'z'))
    d = (c - 'a') + 10;
  else if ((c >= 'A') && (c <= 'Z'))
    d = (c - 'A') + 10;
  else
    return false;  // Not a digit in any base

  if (d >= base)
    return false;  // Not a digit in this base

  tomlparser_consume(tp, 1);
  *digitptr = d;
  return true;
}

static bool tomlparser_parse_integer_base(tomlparser *tp, int64_t *intptr, int base, bool negative)
{
  int d;
  if (!tomlparser_get_digit(tp, &d, base))
  {
    tomlparser_record_error(tp, "Number must start with a digit");
    return false;
  }

  int sign = (negative) ? -1 : 1;
  int64_t val = (d * sign);

  while (1)
  {
    if (tomlparser_peek_char(tp) == '_')
    {
      tomlparser_consume(tp, 1);

      if (!tomlparser_get_digit(tp, &d, base))
      {
        tomlparser_record_error(tp, "Premature end of numeric digits");
        return false;
      }

      val = (val * base) + (d * sign);
    }
    else if (!tomlparser_get_digit(tp, &d, base))
      break;

    val = (val * base) + (d * sign);
  }

  *intptr = val;
  return true;
}

// dec-int = [ minus / plus ] unsigned-dec-int
// unsigned-dec-int = DIGIT / digit1-9 1*( DIGIT / underscore DIGIT )
static bool tomlparser_parse_integer_decimal(tomlparser *tp, int64_t *intptr)
{
  bool negative = false;

  // Optional leading sign
  int c = tomlparser_peek_char(tp);
  if ((c == '-') || (c == '+'))
  {
    negative = (c == '-');
    tomlparser_consume(tp, 1);
    c = tomlparser_peek_char(tp);
  }

  // Extra leading zeroes not allowed, so if first digit is zero we are done
  if (c == '0')
  {
    *intptr = 0;
    tomlparser_consume(tp, 1);
    return true;
  }

  return tomlparser_parse_integer_base(tp, intptr, 10, negative);
}

static bool tomlparser_parse_integer(tomlparser *tp, int64_t *intptr)
{
  if (tomlparser_consume_match(tp, "0b"))  // Binary
    return tomlparser_parse_integer_base(tp, intptr, 2, false);
  else if (tomlparser_consume_match(tp, "0o"))  // Octal
    return tomlparser_parse_integer_base(tp, intptr, 8, false);
  else if (tomlparser_consume_match(tp, "0x"))  // Hex
    return tomlparser_parse_integer_base(tp, intptr, 16, false);

  return tomlparser_parse_integer_decimal(tp, intptr);
}

// special-float = [ minus / plus ] ( inf / nan )
// inf = %x69.6e.66  ; inf
// nan = %x6e.61.6e  ; nan
static bool tomlparser_parse_float_special(tomlparser *tp, double *floatptr)
{
  if (tomlparser_consume_match(tp, "inf") || tomlparser_consume_match(tp, "+inf"))
  {
    *floatptr = INFINITY;
    return true;
  }
  else if (tomlparser_consume_match(tp, "-inf"))
  {
    *floatptr = -INFINITY;
    return true;
  }
  else if (tomlparser_consume_match(tp, "nan") || tomlparser_consume_match(tp, "+nan"))
  {
    *floatptr = NAN;
    return true;
  }
  else if (tomlparser_consume_match(tp, "-nan"))
  {
    *floatptr = -NAN;
    return true;
  }

  return false;
}

// frac = decimal-point zero-prefixable-int
// decimal-point = %x2E               ; .
// zero-prefixable-int = DIGIT *( DIGIT / underscore DIGIT )
static bool tomlparser_parse_float_frac(tomlparser *tp, double *fracptr)
{
  if (!tomlparser_consume_match(tp, "."))
    abort();

  double frac = 0.0;

  int d, p;
  for (p = 0; tomlparser_get_digit(tp, &d, 10); p++)
  {
    frac += (d * pow(10, -p));
  }

  if (p == 0)
  {
    tomlparser_record_error(tp, "Expected digit after decimal point");
    return false;
  }

  *fracptr = frac;
  return true;
}

// float = float-int-part ( exp / frac [ exp ] )
// float =/ special-float
static bool tomlparser_parse_float(tomlparser *tp, double *floatptr)
{
  if (tomlparser_parse_float_special(tp, floatptr))
    return true;

  // Get integer part
  // float-int-part = dec-int
  int64_t intpart;
  if (!tomlparser_parse_integer_decimal(tp, &intpart))
    return false;

  // Get fractional part, if any
  double fracpart = 0;
  if (tomlparser_peek_match(tp, "."))
  {
    if (!tomlparser_parse_float_frac(tp, &fracpart))
      return false;
  }

  // Get exponent part, if any
  // exp = "e" float-exp-part
  // float-exp-part = [ minus / plus ] zero-prefixable-int
  int64_t exppart = 0;
  if (tomlparser_consume_match(tp, "e"))
  {
    bool negative = false;
    int c = tomlparser_peek_char(tp);
    if ((c == '-') || (c == '+'))
    {
      tomlparser_consume(tp, 1);
      negative = (c == '-');
    }

    if (!tomlparser_parse_integer_base(tp, &exppart, 10, negative))
      return false;
  }

  double value = (intpart + fracpart) * pow(10, exppart);
  *floatptr = value;
  return true;
}

// key = simple-key / dotted-key
// simple-key = quoted-key / unquoted-key
// quoted-key = basic-string / literal-string
// dotted-key = simple-key 1*( dot-sep simple-key )
// dot-sep   = ws %x2E ws  ; . Period
static bool tomlparser_parse_key(tomlparser *tp, list **keypath)
{
  int c;

  list *kp = list_new(8);

  bool success = false;
  bytestring *k = bytestring_new(16);

  while ((c = tomlparser_peek_char(tp)) != EOF)
  {
    if (c == '"')
      success = tomlparser_parse_basic_string(tp, k);
    else if (c == '\'')
      success = tomlparser_parse_literal_string(tp, k);
    else
      success = tomlparser_parse_bare_key(tp, k);

    if (!success)
      break;

    list_push(kp, k);
    k = NULL;

    tomlparser_skip_whitespace(tp);

    if (tomlparser_peek_char(tp) != '.')
      break;

    tomlparser_consume(tp, 1);

    tomlparser_skip_whitespace(tp);

    success = false;
    k = bytestring_new(16);
  }

  if (k)
    bytestring_free(k);

  if (success)
    *keypath = kp;
  else
    toml_keypath_free(kp);

  return success;
}

static bool tomlparser_parse_value(tomlparser *tp, tomlvalue *v)
{
  int c = tomlparser_peek_char(tp);

  // The easy cases
  if ((c == '\'') || (c == '"'))
  {
    bytestring *bs = bytestring_new(64);
    if (!tomlparser_parse_string(tp, bs))
    {
      tomlparser_record_error(tp, "Couldn't parse string");
      bytestring_free(bs);
      return false;
    }

    v->type = TOML_TYPE_STR;
    v->u.strval = bs;
    return true;
  }
  else if (c == '[')
    abort();  // TODO: TOML_TYPE_ARRAY
  else if (c == '{')
    abort();  // TODO: TOML_TYPE_TABLE
  else if (tomlparser_consume_match(tp, "true"))
  {
    v->type = TOML_TYPE_BOOL;
    v->u.boolval = true;
    return true;
  }
  else if (tomlparser_consume_match(tp, "false"))
  {
    v->type = TOML_TYPE_BOOL;
    v->u.boolval = false;
    return true;
  }

  // Dates, times, and integers in alternate bases
  int digits = tomlparser_peek_count_set(tp, 0, "0123456789");
  if ((digits == 2) && (buffer_get_byte(tp->peekbuf, 2) == ':'))
    abort();  // TODO: TOML_TYPE_TIME
  else if ((digits == 4) && (buffer_get_byte(tp->peekbuf, 4) == '-'))
    abort();  // TODO: TOML_TYPE_DATE
  else if ((digits == 1) && (c == '0'))
  {
    char c2 = buffer_get_byte(tp->peekbuf, 1);
    if ((c2 == 'b') || (c2 == 'o') || (c2 == 'x'))
    {
      // Alternate-based integer
      int64_t value;
      if (!tomlparser_parse_integer(tp, &value))
        return false;

      v->type = TOML_TYPE_INT;
      v->u.intval = value;
      return true;
    }
  }

  // At this point, either decimal integer or float. First skip over sign
  int digitstart = 0;
  if ((c == '-') || (c == '+'))
    digitstart++;

  // If we have following digits/underscores, skip over a run of them
  digits = tomlparser_peek_count_set(tp, digitstart, "0123456789_");
  if (digits)
  {
    tomlparser_ensure_peekbuf_length(tp, digitstart + digits + 1);
    char c = buffer_get_byte(tp->peekbuf, digitstart + digits + 1);

    if ((c == '.') || (c == 'e') || (c == 'E'))
      abort();  // TODO: TOML_TYPE_FLOAT
    else
    {
      // Decimal integer
      int64_t value;
      if (!tomlparser_parse_integer_decimal(tp, &value))
        return false;

      v->type = TOML_TYPE_INT;
      v->u.intval = value;
      return true;
    }
  }

  // TODO: Must be a special float like 'nan', 'inf', etc.
  abort();
}

//  std-table = std-table-open key std-table-close
//  std-table-open  = %x5B ws     ; [ Left square bracket
//  std-table-close = ws %x5D     ; ] Right square bracket
static bool tomlparser_parse_standard_table_header(tomlparser *tp)
{
  int c = tomlparser_get_char(tp);
  if (c != '[')
    abort();

  tomlparser_skip_whitespace(tp);

  list *keypath;
  if (!tomlparser_parse_key(tp, &keypath))
  {
    tomlparser_record_error(tp, "Could not parse standard table header key");
    return false;
  }

  tomlparser_skip_whitespace(tp);

  if (tomlparser_get_char(tp) != ']')
  {
    toml_keypath_free(keypath);
    tomlparser_record_error(tp, "Expected ']' after standard table header key");
    return false;
  }

  tomlparser_skip_whitespace(tp);

  tomlvalue *node = tomlparser_walk(tp, tp->root, keypath, 0);
  if (!node)
    return false;

  tp->current = node;

  return tomlparser_end_line(tp);
}

//  array-table = array-table-open key array-table-close
//  array-table-open  = %x5B.5B ws  ; [[ Double left square bracket
//  array-table-close = ws %x5D.5D  ; ]] Double right square bracket
static bool tomlparser_parse_array_table_header(tomlparser *tp)
{
  if (!tomlparser_consume_match(tp, "[["))
    abort();

  tomlparser_skip_whitespace(tp);

  list *keypath;
  if (!tomlparser_parse_key(tp, &keypath))
  {
    tomlparser_record_error(tp, "Could not parse array table header key");
    return false;
  }

  tomlparser_skip_whitespace(tp);

  if (!tomlparser_consume_match(tp, "]]"))
  {
    toml_keypath_free(keypath);
    tomlparser_record_error(tp, "Expected ']]' after array table header key");
    return false;
  }

  tomlparser_skip_whitespace(tp);

  tomlvalue *node = tomlparser_walk(tp, tp->root, keypath, -1);
  if (!node)
    return false;

  // Container of array must be a table
  if (node->type != TOML_TYPE_TABLE)
  {
    tomlparser_record_error(tp, "Attempt to use non-table as table");
    return NULL;
  }

  bytestring *key = list_get_item(keypath, list_get_length(keypath) - 1);

  tomlvalue *array = hash_get(node->u.tableval, key);
  if (array == NULL)
  {
    // Auto-vivify array
    array = tomlvalue_new_array();
    hash_add(node->u.tableval, key, array);
  }

  if (array->type != TOML_TYPE_ARRAY)
  {
    tomlparser_record_error(tp, "Attempt to use non-array as array");
    return NULL;
  }

  // Add anonymous table to array
  node = tomlvalue_new_table();
  list_push(array->u.arrayval, node);

  tp->current = node;

  return tomlparser_end_line(tp);
}

// Parses a table header line. There are two forms:
// 1. "[x.y.z]": A regular table with the given key
// 2. "[[x.y.z]]": An anonymous table appended to the array with the given key
static bool tomlparser_parse_header(tomlparser *tp)
{
  if (tomlparser_peek_match(tp, "[["))
    return tomlparser_parse_array_table_header(tp);
  else
    return tomlparser_parse_standard_table_header(tp);
}

static bool tomlparser_parse_assignment(tomlparser *tp)
{
  list *keypath;
  if (!tomlparser_parse_key(tp, &keypath))
  {
    tomlparser_record_error(tp, "Expected key path");
    return false;
  }

  tomlparser_skip_whitespace(tp);

  int c = tomlparser_get_char(tp);
  if (c != '=')
  {
    tomlparser_record_error(tp, "Expected '=' after key path");
    return false;
  }

  tomlparser_skip_whitespace(tp);

  tomlvalue *v = xmalloc(sizeof(tomlvalue));
  v->type = TOML_TYPE_NONE;

  if (!tomlparser_parse_value(tp, v))
  {
    tomlparser_record_error(tp, "Expected value");
    tomlvalue_free(v);
    return false;
  }

  if (!tomlparser_end_line(tp))
  {
    tomlvalue_free(v);
    return false;
  }

  tomlvalue *node = tomlparser_walk(tp, tp->current, keypath, -1);
  if (!node)
  {
    tomlvalue_free(v);
    return false;
  }

  // Container must be a table
  if (node->type != TOML_TYPE_TABLE)
  {
    tomlparser_record_error(tp, "Attempt to store value in non-table");
    tomlvalue_free(v);
    return false;
  }

  // Final key must not exist already
  bytestring *key = list_get_item(keypath, list_get_length(keypath) - 1);
  tomlvalue *old = hash_get(node->u.tableval, key);
  if (old)
  {
    tomlparser_record_error(tp, "Attempt to overwrite populated key");
    tomlvalue_free(v);
    return false;
  }

  // Add key/value to table
  if (!hash_add(node->u.tableval, key, v))
    abort();

  return true;
}

bool tomlparser_parse_statement(tomlparser *tp)
{
  tomlparser_skip_whitespace(tp);

  int c = tomlparser_peek_char(tp);
  if (c == EOF)
    return false;  // EOF

  if (c == '#')  // Full-line comment
  {
    tomlparser_skip_line(tp);
    return true;
  }
  else if ((c == '\x0A') || (c == '\x0D'))  // Blank line
  {
    tomlparser_skip_newline(tp);
    return true;
  }
  else if (c == '[')
  {
    return tomlparser_parse_header(tp);
  }
  else if (tomlparser_parse_assignment(tp))
    return true;

  return false;
}
