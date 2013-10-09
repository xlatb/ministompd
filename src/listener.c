#include <string.h>  // memcpy()
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "ministompd.h"

#define LISTENER_LISTEN_BACKLOG 10

static bool ipv4_str_to_sockaddr(const char *ipaddr, uint16_t port, struct sockaddr_storage *sockaddr)
{
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(addr.sin_family, ipaddr, &addr.sin_addr) != 1)
    return false;

  memcpy(sockaddr, &addr, sizeof(addr));
  return true;
}

static bool ipv6_str_to_sockaddr(const char *ipaddr, uint16_t port, struct sockaddr_storage *sockaddr)
{
  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port);

  if (inet_pton(addr.sin6_family, ipaddr, &addr.sin6_addr) != 1)
    return false;

  memcpy(sockaddr, &addr, sizeof(addr));
  return true;
}

listener *listener_new(void)
{
  // Allocate memory
  listener *l = xmalloc(sizeof(listener));
  l->sockaddr = xmalloc(sizeof(*l->sockaddr));

  // Fill in fields
  l->fd = -1;
  l->status = LISTENER_STATUS_OK;
  l->lasterrno = 0;

  return l;
}

bool listener_set_address(listener *l, const char *ipaddr, uint16_t port)
{
  // Try to decode the IP address
  if (!ipv4_str_to_sockaddr(ipaddr, port, l->sockaddr) &&
      !ipv6_str_to_sockaddr(ipaddr, port, l->sockaddr))
  {
    l->lasterrno = EINVAL;
    return false;  // Could not be decoded
  }

  return true;
}

bool listener_listen(listener *l)
{
  // Sanity check
  if (l->fd >= 0)
    abort();  // Already have a socket

  // Create socket
  int fd = socket(l->sockaddr->ss_family, SOCK_STREAM, 0);
  if (fd == -1)
  {
    l->lasterrno = errno;
    l->status = LISTENER_STATUS_ERROR;
    return false;  // Cannot create socket
  }

  // Remember the socket
  l->fd = fd;

  // Turn on SO_REUSEADDR
  int optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
  {
    l->lasterrno = errno;
    l->status = LISTENER_STATUS_ERROR;
    return false;  // Cannot set SO_REUSEADDR option
  }

  // Turn on O_NONBLOCK
  int flags = fcntl(fd, F_GETFL);
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
  {
    l->lasterrno = errno;
    l->status = LISTENER_STATUS_ERROR;
    return false;  // Cannot set to nonblocking
  }

  // Bind the socket
  if (bind(fd, (struct sockaddr *) l->sockaddr, sizeof(*l->sockaddr)) == -1)
  {
    l->lasterrno = errno;
    l->status = LISTENER_STATUS_ERROR;
    return false;  // Cannot bind
  }

  // Listen on the socket
  if (listen(fd, LISTENER_LISTEN_BACKLOG) == -1)
  {
    l->lasterrno = errno;
    l->status = LISTENER_STATUS_ERROR;
    return false;  // Cannot listen
  }

  // Success
  return true;
}

// Marks fds for watching by a later select() call. Returns the highest known fd.
int listener_mark_fds(listener *l, int highfd, fd_set *readfds, fd_set *writefds)
{
  // Bail out if we're not actually listening
  if (l->status != LISTENER_STATUS_OK)
    return highfd;

  if (highfd < l->fd)
    highfd = l->fd;

  // Select for reading
  FD_SET(l->fd, readfds);

  return highfd;
}

// Accepts a new connection, if possible, and returns it. If there is nothing
//  to accept, returns NULL. If the caller supplies the 'readfds' arg, no
//  work is done unless the listening fd is active in that set.
connection *listener_accept_connection(listener *l, fd_set *readfds)
{
  // Double check the fd_set, if one was supplied
  if ((readfds != NULL) && !FD_ISSET(l->fd, readfds))
    return NULL;  // Our fd was not active in the set

  // Attempt to accept connection
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  int fd = accept(l->fd, (struct sockaddr *)&addr, &addrlen);

  // Handle errors
  if (fd < 0)
  {
    int err = errno;

    if ((err == EAGAIN) || (err == EWOULDBLOCK))
      return NULL;  // Nothing to accept

    // Unexpected error, bail out
    log_perror(LOG_LEVEL_ERROR, "accept()");
    exit(1);
  }

  // Turn on O_NONBLOCK
  int flags = fcntl(fd, F_GETFL);
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
  {
    log_perror(LOG_LEVEL_ERROR, "fcntl()");
    exit(1);
  }

  // Wrap the new connection
  connection *c = connection_new(CONNECTION_STATUS_LOGIN, fd);

  return c;
}

void listener_free(listener *l)
{
  xfree(l->sockaddr);
  xfree(l);
}

