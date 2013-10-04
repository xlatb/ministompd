#include <string.h>  // memmove()
#include <unistd.h>  // read(), write()
#include "ministompd.h"

buffer *buffer_new(size_t size)
{
  buffer *b = xmalloc(sizeof(buffer));
  uint8_t *d = xmalloc(size);

  if (size < 1)
    size = 1;

  b->size = size;
  b->max_size = 0;  // TODO: Implement buffer size limit
  b->length = 0;
  b->position = 0;
  b->data = d;

  return b;
}

// Compacts the data remaining in the buffer to be at the start of the allocated memory.
void buffer_compact(buffer *b)
{
  if (b->position == 0)
    return;  // Nothing to do

  // Reposition data
  if (b->length)
    memmove(b->data, b->data + b->position, b->length);
  b->position = 0;

  return;
}

// Resizes the buffer to at least the given size.
void buffer_resize(buffer *b, size_t size)
{
  size_t new_size;

  new_size = b->size * 2;
  if (new_size < size)
    new_size = size;

  b->size = new_size;
  b->data = xrealloc(b->data, new_size);

  printf("Buffer %p resize to %zd\n", b, new_size);

  // Might as well compact it at the same time
  buffer_compact(b);

  return;
}

// Ensures we have at least the given number of bytes of slack space at the end of the buffer.
void buffer_ensure_slack(buffer *b, size_t new_slack)
{
  int cur_slack = b->size - b->position - b->length;

  if (cur_slack >= new_slack)
    return;  // Nothing to do

  buffer_resize(b, b->size + (new_slack - cur_slack));
}

// Reads data from an fd and adds it to the end of the buffer.
// Return value is the number of bytes consumed from the fd, 0 on EOF, or -1 on error.
ssize_t buffer_input_fd(buffer *b, int fd, size_t size)
{
  // Ensure we have enough slack space to accomodate the read() request
  buffer_ensure_slack(b, size);

  // Read from the fd into the buffer
  int ret = read(fd, b->data + b->position + b->length, size);
  if (ret > 0)
  {
    b->length += ret;
  }

  return ret;
}

// Reads data from a string of bytes.
// Returns the number of bytes read.
ssize_t buffer_input_bytes(buffer *b, const uint8_t *bytes, size_t size)
{
  // Ensure we have enough slack space
  buffer_ensure_slack(b, size);

  // Add the bytes to the end of the buffer
  memcpy(b->data + b->position + b->length, bytes, size);
  b->length += size;

  return size;
}

// Reads a single byte into the buffer.
// Returns the number of bytes read.
ssize_t buffer_input_byte(buffer *b, uint8_t byte)
{
  // Ensure we have enough slack space
  buffer_ensure_slack(b, 1);

  // Add the byte
  b->data[b->position + b->length] = byte;
  b->length++;

  return 1;
}

// Reads a bytestring into the buffer.
// Returns the number of bytes read.
ssize_t buffer_input_bytestring(buffer *b, const bytestring *bs)
{
  return buffer_input_bytes(b, bs->data, bs->length);
}

// Reads a segment of a bytestring into the buffer.
// Returns the number of bytes read.
ssize_t buffer_input_bytestring_slice(buffer *b, const bytestring *bs, int position, size_t length)
{
  // Bounds check
  if ((position < 0) || (position > bs->length))
    return 0;  // Out of bounds

  // Don't write more bytes than are in the string
  if ((position + length) > bs->length)
    length = bs->length - position;

  return buffer_input_bytes(b, bs->data + position, length);
}

// Writes data to an fd and removes it from the start of the buffer.
// Return value is the number of bytes consumed by the fd, or -1 on error.
ssize_t buffer_output_fd(buffer *b, int fd, size_t size)
{
  // Bounds check
  if (size > b->length)
    size = b->length;

  // Don't bother if there's nothing to send
  if (size == 0)
    return 0;

  // Write data to the fd
  int ret = write(fd, b->data + b->position, size);
  if (ret > 0)
  {
    b->position += ret;
    b->length -= ret;
  }

  // If buffer is empty now, do trivial compaction
  if (b->length == 0)
    b->position = 0;

  return ret;
}

// Returns the relative position of the first occurance of the given byte from
//  the current position, or -1 if no such byte is found.
int buffer_find_byte(buffer *b, uint8_t byte)
{
  uint8_t *p = memchr(b->data + b->position, byte, b->length);
  if (!p)
    return -1;  // No byte found

  return (p - (b->data + b->position));
}

// Like buffer_find_byte(), but the start position and length of the search
//  is explicit.
int buffer_find_byte_within(buffer *b, uint8_t byte, int position, size_t length)
{
  // Don't search if the start position is out of bounds
  if ((position < 0) || (position > (b->position + b->length)))
    return -1;

  // Limit the length to the actual amount of data
  if (length > (b->length - position))
    length = (b->length - position);

  uint8_t *p = memchr(b->data + b->position + position, byte, length);
  if (!p)
    return -1;  // No byte found

  return (p - (b->data + b->position));
}

// Append the given bytes from the buffer to the end of a bytestring. The
//  bytes in the buffer are not consumed.
void buffer_append_bytestring(buffer *b, bytestring *bs, int position, size_t length)
{
  // Bounds check
  if ((position < 0) || ((position + length) > b->length))
    abort();

  // Extract bytestring
  bytestring_append_bytes(bs, b->data + b->position + position, length);

  return;
}

// Discard the given number of bytes on the reader side.
void buffer_consume(buffer *b, int count)
{
  if (count < b->length)
  {
    b->position += count;
    b->length -= count;
    return;
  }

  // We discarded all the data. Might as well re-position.
  b->position = 0;
  b->length = 0;
  return;
}

uint8_t buffer_get_byte(buffer *b, int index)
{
  // Bounds check
  if ((index < 0) || (index >= b->length))
    return 0;

  // Return byte
  return b->data[b->position + index];
}

size_t buffer_get_length(buffer *b)
{
  return b->length;
}

void buffer_dump(buffer *b)
{
  printf("Buffer %p size %zd length %zd\n", b, b->size, b->length);
  write(STDOUT_FILENO, b->data + b->position, b->length);
}

void buffer_free(buffer *b)
{
  xfree(b->data);
  xfree(b);
}
