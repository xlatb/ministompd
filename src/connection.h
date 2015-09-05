#include "queuetypes.h"
#include <sys/time.h>  // struct timeval

#ifndef MINISTOMPD_CONNECTION_H
#define MINISTOMPD_CONNECTION_H

enum connection_status
{
  CONNECTION_STATUS_CLOSED,
  CONNECTION_STATUS_LOGIN,       // Waiting for login credentials
  CONNECTION_STATUS_CONNECTED,
  CONNECTION_STATUS_STOMP_ERROR  // Push out an ERROR frame and then disconnect
};

enum connection_version
{
  CONNECTION_VERSION_1_2  // STOMP 1.2
};

typedef struct
{
  enum connection_status  status;           // Current status
  enum connection_version version;          // STOMP version used for this connection
  int                     error;            // If the connection was aborted by a socket error, contains the errno
  int                     fd;               // Underlying fd
  int                     inheartbeat;      // Negotiated incoming heartbeat frequency
  int                     outheartbeat;     // Negotiated outgoing heartbeat frequency
  struct timeval          readtime;         // Time of last read() returning data from underlying socket
  struct timeval          writetime;        // Time of last successful write() to underlying socket
  buffer                 *inbuffer;         // Input buffer
  buffer                 *outbuffer;        // Output buffer
  frameparser            *frameparser;      // Frame parser
  frameserializer        *frameserializer;  // Frame serializer
  hash                   *subscriptionmap;  // Subscription map (id -> subscription)
} connection;

connection  *connection_new(enum connection_status status, int fd);
void         connection_free(connection *c);
bool         connection_subscribe(connection *c, subscription *sub);
void         connection_close(connection *c);
void         connection_pump_input(connection *c);
void         connection_pump_output(connection *c);
void         connection_send_error_message(connection *c, frame *causalframe, bytestring *msg);
void         connection_dump(connection *c);

#endif
