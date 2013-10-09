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

// Pull waiting input in to the connection's buffer.
void connection_pump_input(connection *c)
{
  int readcount = 0;

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

  // Update read timestamp, if needed
  if (readcount > 0)
    gettimeofday(&c->readtime, NULL);
}

// Push waiting output out to the socket.
void connection_pump_output(connection *c)
{
  int writecount = 0;

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

  // Update write timestamp, if needed
  if (writecount > 0)
    gettimeofday(&c->writetime, NULL);

}

// Puts the connection in error status and queues an ERROR frame for output.
//  The causalframe should contain the client frame that casued the error,
//  or NULL if there is none. Takes ownership of the error message bytestring.
void connection_send_error_message(connection *c, frame *causalframe, bytestring *msg)
{
  // Set error connection status
  c->status = CONNECTION_STATUS_STOMP_ERROR;

  // Create an error frame containing the message
  frame *errorframe = frame_new();
  frame_set_command(errorframe, CMD_ERROR);

  // Set error message header
  headerbundle *errorheaders = frame_get_headerbundle(errorframe);
  headerbundle_append_header(errorheaders, bytestring_new_from_string("message"), msg);

  // If there was a causal frame, add details from it
  if (causalframe)
  {
    // Original 'receipt' header is included as the error's 'receipt-id' header
    const bytestring *receipt = headerbundle_get_header_value_by_str(frame_get_headerbundle(causalframe), "receipt");
    if (receipt)
    {
      headerbundle_append_header(errorheaders, bytestring_new_from_string("receipt-id"), bytestring_dup(receipt));;
    }
  }

  // Enqueue frame
  if (!frameserializer_enqueue_frame(c->frameserializer, errorframe))
  {
    log_printf(LOG_LEVEL_ERROR, "Outgoing queue is full, dropping error frame.\n");
    connection_close(c);
    return;
  }

  return;
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

