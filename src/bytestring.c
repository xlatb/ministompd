#include <string.h>  // memcpy(), memchr()
#include <ctype.h>   // isprint()

#include "ministompd.h"

bytestring *bytestring_new(size_t size)
{
  // Size must be positive
  if (size < 1)
    size = 1;

  // Round size to the next multiple of 8 bytes
  size = (size | 0x7) + 1;

  bytestring *bs = xmalloc(sizeof(bytestring));
  uint8_t *d = xmalloc(size);

  bs->size = size;
  bs->length = 0;
  bs->data = d;

  return bs;
}

bytestring *bytestring_new_from_string(const char *str)
{
  size_t len = strlen(str);
  bytestring *bs = bytestring_new(len);
  bytestring_set_bytes(bs, (const uint8_t *)str, len);

  return bs;
}

// Duplicates a bytestring
bytestring *bytestring_dup(const bytestring *bs)
{
  bytestring *dup = bytestring_new(bs->length);
  bytestring_set_bytes(dup, bs->data, bs->length);

  return dup;
}

bytestring *bytestring_resize(bytestring *bs, size_t size)
{
  // If already the right size, do nothing
  if (size == bs->size)
    return bs;

  // If we are about to shrink, decrease the length to fit
  if (bs->length > size)
    bs->length = size;

  // Round size to the next multiple of 8 bytes
  size = (size | 0x7) + 1;

  bs->data = xrealloc(bs->data, size);

  bs->size = size;

  return bs;
}

// Replace the contents of the bytestring with the given bytes.
bytestring *bytestring_set_bytes(bytestring *bs, const uint8_t *data, size_t length)
{
  // Ensure enough space to hold all data
  if (bs->size < length)
    bytestring_resize(bs, length);

  // Copy the data
  memcpy(bs->data, data, length);
  bs->length = length;

  return bs;
}

// Append the given bytes to the end of the bytestring.
bytestring *bytestring_append_bytes(bytestring *bs, const uint8_t *data, size_t length)
{
  // Ensure enough space to hold all data
  if (bs->size < (bs->length + length))
    bytestring_resize(bs, (bs->length + length));

  // Append the data to the end
  memcpy(bs->data + bs->length, data, length);
  bs->length += length;

  return bs;
}

// Append a single byte to the end of the bytestring.
bytestring *bytestring_append_byte(bytestring *bs, uint8_t byte)
{
  // Ensure enough space to hold all data
  if (bs->size < (bs->length + 1))
    bytestring_resize(bs, (bs->length + 1));

  // Append the data to the end
  bs->data[bs->length] = byte;
  bs->length++;

  return bs;
}

// Append one bytestring to another.
bytestring *bytestring_append_bytestring(bytestring *bs, const bytestring *src)
{
  // Ensure size
  if (bs->size < (bs->length + src->length))
    bytestring_resize(bs, bs->length + src->length);

  // Append data
  memcpy(bs->data + bs->length, src->data, src->length);
  bs->length += src->length;

 return bs;
}

// Append data to one bytestring from another, using the given source position and length.
bytestring *bytestring_append_bytestring_fragment(bytestring *bs, const bytestring *src, int position, size_t length)
{
  // Start position bounds check
  if ((position < 0) || (position >= src->length))
    return bs;  // Nothing to do

  // Length bounds check
  if (position + length > src->length)
    length = src->length - position;

  // Ensure enough space to hold all data
  if (bs->size < (bs->length + length))
    bytestring_resize(bs, (bs->length + length));

  // Append the data to the end
  memcpy(bs->data + bs->length, src->data + position, length);
  bs->length += length;

  return bs;
}

// Append printf()-style string data to the end of a bytestring.
bytestring *bytestring_append_printf(bytestring *bs, const char *fmt, ...)
{
  va_list args;

  while (1)
  {
    int count;
    int slack = bs->size - bs->length;

    // Attempt the printf() with whatever slack space we have left
    va_start(args, fmt);
    count = vsnprintf((char *) bs->data + bs->length, slack, fmt, args);
    va_end(args);

    // Unexpected failure
    if (count < 0)
      abort();

    // If there was enough space, we're all done
    if (count < slack)
    {
      bs->length += count;
      return bs;
    }

    // Add the required amount of slack space
    bytestring_resize(bs, bs->length + count + 1);  // +1 because we need space for NUL byte not included in count
  }
}

// As above, but don't use varargs.
bytestring *bytestring_append_vprintf(bytestring *bs, const char *fmt, va_list args)
{
  int count;
  int slack = bs->size - bs->length;

  // Make a copy of the original args, in case we need to retry
  va_list original_args;
  va_copy(original_args, args);

  // Attempt the printf() with whatever slack space we have left
  count = vsnprintf((char *) bs->data + bs->length, slack, fmt, args);

  // Unexpected failure
  if (count < 0)
    abort();

  // If there was enough space, we're all done
  if (count < slack)
  {
    va_end(original_args);
    bs->length += count;
    return bs;
  }

  // Add the required amount of slack space
  bytestring_resize(bs, bs->length + count + 1);  // +1 because we need space for NUL byte not included in count
  slack = bs->size - bs->length;

  // Retry the printf()
  count = vsnprintf((char *) bs->data + bs->length, slack, fmt, original_args);
  va_end(original_args);

  // This should not happen, since we allocated enough space
  if ((count < 0) || (count >= slack))
    abort();

  // Fix up length
  bs->length += count;

  return bs;
}

// Returns the position of the next occurance of the given byte starting at
//  the given position. Returns -1 if the byte does not occur.
int bytestring_find_byte(const bytestring *bs, uint8_t byte, int startpos)
{
  // Bounds check
  if ((startpos < 0) || (startpos >= bs->length))
    return -1;  // Out of bounds

  // Find it
  uint8_t *p = memchr(bs->data + startpos, byte, bs->length - startpos);
  if (!p)
    return -1;  // No byte found

  return (p - bs->data);
}

bool bytestring_equals(const bytestring *bs1, const bytestring *bs2)
{
  if (bs1->length != bs2->length)
    return false;  // Can't be equal if lengths differ

  return (memcmp(bs1->data, bs2->data, bs1->length) == 0);
}

// Returns true if the bytestring equals the given bytes.
bool bytestring_equals_bytes(const bytestring *bs, const uint8_t *data, size_t length)
{
  if (bs->length != length)
    return false;  // Can't be equal if lengths differ

  return (memcmp(bs->data, data, length) == 0);
}

// Performs a three-way comparison between the bytestring and a C string.
int bytestring_cmp_string(const bytestring *bs, const char *p2)
{
  uint8_t *p1 = bs->data;
  uint8_t *p1end = bs->data + bs->length;

  // While neither string has terminated, and the bytes match, skip forward
  while (*p2 && (p1 < p1end) && (*p1 == *p2))
  {
    p1++;
    p2++;
  }

  if (p1 >= p1end)
  {
    if (*p2)
      return -1;  // Byte string terminated but C string hasn't, C string is greater
    else
      return 0;  // Both have terminated
  }
  else if (!*p2)
  {
    return 1;  // Byte string hasn't terminated but C string has, C string is lesser
  }
  else if (*p1 < *p2)
  {
    return -1;  // Byte string is greater
  }
  else
  {
    return 1;  // C string is greater
  }
}

// Returns true if we were able to read a numeric string from the bytestring,
//  starting at the given start position, in the given base. If 'end' is not
//  NULL, fills in the ending position of the numeric string. The end position
//  may be one byte past the end of the bytestring, if the entire string was
//  numeric.
bool bytestring_strtol(const bytestring *bs, int start, int *end, long *value, int base)
{
  long v = 0;
  int p = start;
  int m = 10;  // Multiplier
  uint8_t byte;

  // Bounds check the start position
  if (start >= bs->length)
    return false;

  // Check base
  if (base != 10)
    abort();  // Unimplemented

  // Check for leading sign
  byte = bs->data[p];
  if (byte == '+')
    p++;  // Redundant, but okay
  else if (byte == '-')
  {
    m = -10;
    p++;
  }

  // Read digits of the number
  int count = 0;
  while (p < bs->length)
  {
    byte = bs->data[p];
    if ((byte < '0') || (byte > '9'))
      break;  // Not a digit

    // Process current character
    v = (v * m) + (byte - '0');
    p++;
    count++;

    // Overflow/underflow check
    if ((m > 0) && (v < 0))
      return false;  // Underflow
    else if ((m < 0) && (v > 0))
      return false;  // Overflow
  }

  // Make sure we got at least one digit (not just a bare sign)
  if (count == 0)
    return false;

  // Success
  if (end != NULL)
    *end = p;
  if (value != NULL)
    *value = v;
  return true;
}

void bytestring_dump(const bytestring *b)
{
  printf("bytestring %p size %zd length %zd: ", b, b->size, b->length);

  for (int i = 0; i < b->length; i++)
  {
    uint8_t byte = b->data[i];

    if (byte == '\\')
      fputs("\\\\", stdout);
    else if (isprint(byte))
      putchar(byte);
    else
      printf("\\x%02X", byte);
  }

  putchar('\n');
}

void bytestring_free(bytestring *b)
{
  xfree(b->data);
  xfree(b);
}
