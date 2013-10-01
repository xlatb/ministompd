#include <string.h>  // memset()

#include "ministompd.h"

connectionbundle *connectionbundle_new(void)
{
  // Allocate memory
  connectionbundle *cb = xmalloc(sizeof(connectionbundle));

  // Fill in fields
  cb->size        = 16;  // A reasonable starting size
  cb->count       = 0;
  cb->connections = xmalloc(sizeof(connection *) * cb->size);

  return cb;
}

void connectionbundle_add_connection(connectionbundle *cb, connection *c)
{
  // Resize if needed
  if (cb->count == cb->size)
  {
    cb->size *= 2;
    cb->connections = xrealloc(cb->connections, sizeof(connection *) * cb->size);
    memset(cb->connections + cb->count, 0, sizeof(connection *) * (cb->size - cb->count));  // Clear new slots
  }

  // Find free slot
  int slot;
  for (int s = 0; s < cb->size; s++)
  {
    if (cb->connections[s] == NULL)
      slot = s;
  }

  // Add connection to slot
  cb->connections[slot] = c;

  return;
}

// Marks fds for watching by a later select() call. Returns the highest known fd.
int connectionbundle_mark_fds(connectionbundle *cb, int highfd, fd_set *readfds, fd_set *writefds)
{
  for (int s = 0; s < cb->size; s++)
  {
    connection *c = cb->connections[s];
    if (c == NULL)
      continue;

    // Track the highest-numbered fd
    if (highfd < c->fd)
      highfd = c->fd;

    // Always select for reading
    FD_SET(c->fd, readfds);

    // Select for writing if the write buffer is not empty
    if (buffer_get_length(c->outbuffer) > 0)
      FD_SET(c->fd, writefds);
  }

  return highfd;
}

cb_iter connectionbundle_iter_new(connectionbundle *cb)
{
  return 0;  // Next slot
}

connection *connectionbundle_get_next_active_connection(connectionbundle *cb, cb_iter *iter, fd_set *readfds, fd_set *writefds)
{
  for (int s = *iter; s < cb->size; s++)
  {
    connection *c = cb->connections[s];
    if (c == NULL)
      continue;

    if (FD_ISSET(c->fd, readfds) || FD_ISSET(c->fd, writefds))
    {
      *iter = s + 1;
      return c;
    }
  }

  return NULL;  // No more connections to check
}

// TODO: Clean up the constituent connections also
void connectionbundle_free(connectionbundle *cb)
{
  xfree(cb->connections);
  xfree(cb);
}

