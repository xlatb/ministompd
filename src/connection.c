#include <sys/time.h>  // gettimeofday()
#include <errno.h>

#include "ministompd.h"

// Abort the connection due to a socket error.
static void connection_abort(connection *c, int error)
{
  c->status = CONNECTION_STATUS_CLOSED;
  close(c->fd);

  if (c->error != 0)
    c->error = error;
}

connection *connection_new(enum connection_status status, int fd)
{
  // Allocate memory
  connection *c = xmalloc(sizeof(connection));

  // Get connection timestamp
  struct timeval connecttime;
  gettimeofday(&connecttime, NULL);

  // Fill in fields
  c->status          = status;
  c->version         = CONNECTION_VERSION_1_2;
  c->error           = 0;
  c->fd              = fd;
  c->inheartbeat     = 0;
  c->outheartbeat    = 0;
  c->readtime        = connecttime;
  c->writetime       = connecttime;
  c->inbuffer        = buffer_new(4096);
  c->outbuffer       = buffer_new(4096);
  c->frameparser     = frameparser_new();
  c->frameserializer = frameserializer_new();

  return c;
}

void connection_close(connection *c)
{
  c->status = CONNECTION_STATUS_CLOSED;
  close(c->fd);
}

// Push waiting I/O through the connection.
void connection_pump(connection *c)
{
  int readcount = 0;
  int writecount = 0;

  // Try to read some data
  readcount = buffer_input_fd(c->inbuffer, c->fd, NETWORK_READ_SIZE);
  if (readcount == 0)
  {
    connection_close(c);
  }
  else if (readcount < 0)
  {
    int error = errno;
    if ((error != EAGAIN) && (error != EWOULDBLOCK))
      connection_abort(c, error);  // Unexpected error
  }

  // If we have data waiting to go out, try writing it
  size_t outbuflen = buffer_get_length(c->outbuffer);
  if (outbuflen > 0)
  {
    writecount = buffer_output_fd(c->outbuffer, c->fd, outbuflen);
    if (writecount < 0)
    {
      int error = errno;
      if (error == EPIPE)
        connection_close(c);
      else if ((error != EAGAIN) && (error != EWOULDBLOCK))
        connection_abort(c, error);  // Unexpected error
    }
  }

  // Update timestamps
  if (readcount || writecount)
  {
    // Get current time
    struct timeval iotime;
    gettimeofday(&iotime, NULL);

    // Update read timestamp if needed
    if (readcount > 0)
      c->readtime = iotime;

    // Update write timestamp if needed
    if (writecount > 0)
      c->writetime = iotime;
  }
}

void connection_dump(connection *c)
{
  printf("Connection %p fd %d status %d\n", c, c->fd, c->status);
}

void connection_free(connection *c)
{
  frameparser_free(c->frameparser);
  frameserializer_free(c->frameserializer);
  buffer_free(c->inbuffer);
  buffer_free(c->outbuffer);
  xfree(c);
}

