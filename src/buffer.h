#include <stdlib.h>  // size_t
#include <unistd.h>  // ssize_t
#include <stdint.h>  // uint8_t

#include "bytestring.h"

#ifndef MINISTOMPD_BUFFER_H
#define MINISTOMPD_BUFFER_H

typedef struct
{
  size_t   size;      // Allocated bytes
  size_t   max_size;  // Size we're willing to grow to

  size_t   length;    // Current number of bytes stored
  int      position;  // Current position of first byte

  uint8_t *data;      // Bytes stored
} buffer;

buffer *buffer_new(size_t size);
void    buffer_clear(buffer *b);
void    buffer_compact(buffer *b);
void    buffer_resize(buffer *b, size_t size);
void    buffer_ensure_slack(buffer *b, size_t new_slack);
ssize_t buffer_input_fd(buffer *b, int fd, size_t size);
ssize_t buffer_write_bytes(buffer *b, const uint8_t *bytes, size_t size);
ssize_t buffer_write_byte(buffer *b, uint8_t byte);
ssize_t buffer_write_bytestring(buffer *b, const bytestring *bs);
ssize_t buffer_write_bytestring_slice(buffer *b, const bytestring *bs, int position, size_t length);
ssize_t buffer_output_fd(buffer *b, int fd, size_t size);
int     buffer_find_byte(buffer *b, uint8_t byte);
int     buffer_find_first_byte_in_set(buffer *b, uint8_t *set, size_t setlen, uint8_t *found);
int     buffer_find_byte_within(buffer *b, uint8_t byte, int position, size_t length);
void    buffer_append_bytestring(buffer *b, bytestring *bs, int position, size_t length);
void    buffer_consume(buffer *b, int bytes);
uint8_t buffer_get_byte(buffer *b, int index);
size_t  buffer_get_length(buffer *b);
void    buffer_dump(buffer *b);
void    buffer_free(buffer *b);

#endif
