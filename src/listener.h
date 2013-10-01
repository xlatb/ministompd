#include <sys/select.h>  // fd_set

#ifndef MINISTOMPD_LISTENER_H
#define MINISTOMPD_LISTENER_H

enum listener_status
{
  LISTENER_STATUS_OK,
  LISTENER_STATUS_ERROR
};

typedef struct
{
  int                      fd;         // Listening fd
  enum listener_status     status;     // Current status
  int                      lasterrno;  // Errno from last known error
  struct sockaddr_storage *sockaddr;   // Socket address to listen on
} listener;

listener   *listener_new(void);
bool        listener_set_address(listener *l, const char *ipaddr, uint16_t port);
bool        listener_listen(listener *l);
int         listener_mark_fds(listener *l, int highfd, fd_set *readfds, fd_set *writefds);
connection *listener_accept_connection(listener *l, fd_set *readfds);
void        listener_free(listener *l);

#endif
