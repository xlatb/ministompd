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

listener *listener_new(void);
bool      listener_set_address(listener *l, const char *ipaddr, uint16_t port);
bool      listener_listen(listener *l);
void      listener_free(listener *l);

#endif
