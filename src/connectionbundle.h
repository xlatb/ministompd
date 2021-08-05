#ifndef MINISTOMPD_CONNECTIONBUNDLE_H
#define MINISTOMPD_CONNECTIONBUNDLE_H

typedef struct
{
  int          size;         // Number of slots allocated
  int          count;        // Number of slots filled
  connection **connections;  // Array of pointers to connections
} connectionbundle;

typedef int cb_iter;

connectionbundle *connectionbundle_new(void);
void              connectionbundle_add_connection(connectionbundle *cb, connection *c);
int               connectionbundle_mark_fds(connectionbundle *cb, int highfd, fd_set *readfds, fd_set *writefds);
cb_iter           connectionbundle_iter_new(connectionbundle *cb);
connection       *connectionbundle_get_next_connection(connectionbundle *cb, cb_iter *iter);
connection       *connectionbundle_get_next_active_connection(connectionbundle *cb, cb_iter *iter, fd_set *readfds, fd_set *writefds);
connection       *connectionbundle_reap_next_connection(connectionbundle *cb, cb_iter *iter);
void              connectionbundle_free(connectionbundle *cb);

#endif
