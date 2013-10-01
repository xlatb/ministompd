#include <sys/time.h>  // struct timeval

#ifndef MINISTOMPD_CONNECTION_H
#define MINISTOMPD_CONNECTION_H

enum connection_status
{
  CONNECTION_STATUS_CLOSED,
  CONNECTION_STATUS_ERROR,
  CONNECTION_STATUS_LOGIN,
  CONNECTION_STATUS_CONNECTED
};

enum connection_version
{
  CONNECTION_VERSION_1_2  // STOMP 1.2
};

typedef struct
{
  enum connection_status  status;        // Current status
  enum connection_version version;       // STOMP version used for this connection
  int                     error;         // If in error status, contains the errno
  int                     fd;            // Underlying fd
  int                     inheartbeat;   // Negotiated incoming heartbeat frequency
  int                     outheartbeat;  // Negotiated outgoing heartbeat frequency
  struct timeval          readtime;      // Time of last read() returning data from underlying socket
  struct timeval          writetime;     // Time of last successful write() to underlying socket
  buffer                 *inbuffer;      // Input buffer
  buffer                 *outbuffer;     // Output buffer
  frameparser            *frameparser;   // Frame parser
} connection;

connection *connection_new(enum connection_status status, int fd);
void        connection_pump(connection *c);
void        connection_dump(connection *c);
void        connection_free(connection *c);

#endif
