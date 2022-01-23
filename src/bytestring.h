#include <stdlib.h>   // size_t
#include <stdint.h>   // uint8_t
#include <stdarg.h>   // varargs
#include <stdbool.h>  // bool
#include <unistd.h>

#ifndef MINISTOMPD_BYTESTRING_H
#define MINISTOMPD_BYTESTRING_H

typedef struct
{
  size_t   size;      // Allocated bytes
  size_t   length;    // Current number of bytes stored
  uint8_t *data;      // Data bytes
} bytestring;

bytestring *bytestring_new(size_t size);
bytestring *bytestring_new_from_string(const char *str);
bytestring *bytestring_dup(const bytestring *bs);
bytestring *bytestring_resize(bytestring *bs, size_t size);
bytestring *bytestring_set_bytes(bytestring *bs, const uint8_t *data, size_t length);
bytestring *bytestring_append_bytes(bytestring *bs, const uint8_t *data, size_t length);
bytestring *bytestring_append_byte(bytestring *bs, uint8_t byte);
bytestring *bytestring_append_bytestring(bytestring *bs, const bytestring *src);
bytestring *bytestring_append_bytestring_fragment(bytestring *bs, const bytestring *src, int position, size_t length);
bytestring *bytestring_printf(bytestring *pb, const char *fmt, ...);
bytestring *bytestring_vprintf(bytestring *pb, const char *fmt, va_list args);
int         bytestring_find_byte(const bytestring *b, uint8_t byte, int startpos);
bool        bytestring_equals(const bytestring *bs1, const bytestring *bs2);
bool        bytestring_equals_bytes(const bytestring *bs, const uint8_t *data, size_t length);
int         bytestring_cmp_string(const bytestring *bs, const char *p2);
bool        bytestring_strtol(const bytestring *bs, int start, int *end, long *value, int base);
ssize_t     bytestring_write_fd(const bytestring *bs, int fd);
void        bytestring_dump(const bytestring *b);
void        bytestring_free(bytestring *b);

static inline const uint8_t *bytestring_get_bytes(const bytestring *bs)
{
  return (const uint8_t *) bs->data;
}

static inline uint8_t bytestring_get_byte(const bytestring *bs, unsigned int i)
{
  return ((i >= bs->length) ? 0 : (bs->data[i]));
}

static inline size_t bytestring_get_length(const bytestring *bs)
{
  return bs->length;
}

static inline void bytestring_truncate(bytestring *bs, size_t length)
{
  if (bs->length >= length)
    bs->length = length;
}

static inline ssize_t bytestring_write_stdout(const bytestring *bs)
{
  return bytestring_write_fd(bs, STDOUT_FILENO);
}

static inline ssize_t bytestring_write_stderr(const bytestring *bs)
{
  return bytestring_write_fd(bs, STDERR_FILENO);
}

static inline void bytestring_ensure_size(bytestring *bs, size_t size)
{
  if (bs->size < size)
    bytestring_resize(bs, size);
}

#endif
