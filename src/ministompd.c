#include <sys/types.h>  // open()
#include <sys/stat.h>   // open()
#include <fcntl.h>      // open()
#include <sys/select.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>  // STDIN_FILENO
#include <signal.h>  // signal()
#include <string.h>  // strerror()
#include "ministompd.h"

listener *l;

queue *q;

void loop(void);
void parse_file(char *filename);
void handle_connection(connection *c);
void handle_connection_input(connection *c);
void handle_connection_input_frame(connection *c, frame *f);
void handle_connection_output(connection *c);
void reap_connection(connection *c);

int main(int argc, char *argv[])
{
  log_printf(LOG_LEVEL_INFO, "Starting up.\n");

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  // Create a new listener
  l = listener_new();
  if (!listener_set_address(l, "::1", 61613))
  {
    log_printf(LOG_LEVEL_ERROR, "Couldn't set listening address.\n");
    exit(1);
  }
  else if (!listener_listen(l))
  {
    log_printf(LOG_LEVEL_ERROR, "Couldn't listen on socket.\n");
    exit(1);
  }

  // Create queue
  queueconfig *qc = queueconfig_new();
  q = queue_new(bytestring_new_from_string("test"), qc);

  // Run event loop
  loop();

  // Clean up listener
  listener_free(l);

  return 0;
}

void loop(void)
{
  connectionbundle *cb = connectionbundle_new();

  bool done = false;

  while (!done)
  {
    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    int highfd = 0;

    // Mark fds to watch
    highfd = listener_mark_fds(l, highfd, &readfds, &writefds);
    highfd = connectionbundle_mark_fds(cb, highfd, &readfds, &writefds);

    struct timeval timeout = {30, 0};  // Thirty seconds

    // Wait for activity on fds
    int count = select(highfd + 1, &readfds, &writefds, NULL, &timeout);
    log_printf(LOG_LEVEL_DEBUG, "Select returned: %d\n", count);

    // Give up if the select() didn't work
    if (count < 0)
    {
      log_perror(LOG_LEVEL_DEBUG, "select()");
      exit(1);
    }

    // If nothing happened, keep waiting
    if (count == 0)
      continue;

    // Check for new connections
    connection *c = listener_accept_connection(l, &readfds);
    if (c != NULL)
    {
      log_printf(LOG_LEVEL_INFO, "New connection %p accepted.\n", c);
      connectionbundle_add_connection(cb, c);
    }

    // Check for activity on existing connections
    cb_iter iter = connectionbundle_iter_new(cb);
    while ((c = connectionbundle_get_next_active_connection(cb, &iter, &readfds, &writefds)))
    {
      handle_connection(c);
    }

    // Check for closed connections
    iter = connectionbundle_iter_new(cb);
    while ((c = connectionbundle_reap_next_connection(cb, &iter)))
    {
      reap_connection(c);
    }
  }
}

void handle_connection(connection *c)
{
  log_printf(LOG_LEVEL_DEBUG, "Connection %p is interesting.\n", c);
  connection_dump(c);

  connection_pump_input(c);

  if ((c->status == CONNECTION_STATUS_LOGIN) || (c->status == CONNECTION_STATUS_CONNECTED))
    handle_connection_input(c);

  if ((c->status == CONNECTION_STATUS_CONNECTED) || (c->status == CONNECTION_STATUS_STOMP_ERROR))
    handle_connection_output(c);

  connection_pump_output(c);
}

void handle_connection_input(connection *c)
{
  frameparser_outcome outcome = frameparser_parse(c->frameparser, c->inbuffer);
  log_printf(LOG_LEVEL_DEBUG, "Parse: %d\n", outcome);

  if (outcome == FP_OUTCOME_ERROR)
  {
    log_printf(LOG_LEVEL_ERROR, "-- Parse error: ");
    bytestring_dump(frameparser_get_error(c->frameparser));
    connection_send_error_message(c, NULL, bytestring_dup(frameparser_get_error(c->frameparser)));
  }
  else if (outcome == FP_OUTCOME_FRAME)
  {
    frame *f = frameparser_get_frame(c->frameparser);
    log_printf(LOG_LEVEL_DEBUG, "-- Completed frame: ");
    frame_dump(f);

    // Process the frame
    handle_connection_input_frame(c, f);
  }

}

void handle_connection_input_frame(connection *c, frame *f)
{
  if (c->status == CONNECTION_STATUS_LOGIN)
  {
    if ((f->command == CMD_STOMP) || (f->command == CMD_CONNECT))
    {
      frame *f = frame_new();
      frame_set_command(f, CMD_CONNECTED);
      headerbundle *hb = frame_get_headerbundle(f);
      headerbundle_append_header(hb, bytestring_new_from_string("version"), bytestring_new_from_string("1.2"));
      if (!frameserializer_enqueue_frame(c->frameserializer, f))
        abort();  // Couldn't enqueue CONNECTED frame
      c->status = CONNECTION_STATUS_CONNECTED;
    }
    else
    {
      connection_send_error_message(c, f, bytestring_new_from_string("Expected STOMP or CONNECT frame"));
    }
    return;
  }
  else
  {
    //// Echo frame back to client
    //frameserializer_enqueue_frame(c->frameserializer, f);

    // Add frame to test queue
    queue_enqueue(q, f);
  }
}

void handle_connection_output(connection *c)
{
  frameserializer_serialize(c->frameserializer, c->outbuffer);


}

void reap_connection(connection *c)
{
  if (c->error)
    log_printf(LOG_LEVEL_ERROR, "Connection %p closed due to error: %s\n", c, strerror(c->error));
  else
    log_printf(LOG_LEVEL_INFO, "Connection %p has closed normally.\n", c);

  connection_free(c);
}

void parse_file(char *filename)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    log_perror(LOG_LEVEL_ERROR, "open()");
    exit(1);
  }

  frameparser *fp = frameparser_new();

  buffer *b = buffer_new(4096);

  while (buffer_input_fd(b, fd, 4096) > 0)
  {
    log_printf(LOG_LEVEL_DEBUG, "Read loop...\n");

    frameparser_outcome outcome = frameparser_parse(fp, b);
    log_printf(LOG_LEVEL_DEBUG, "Parse: %d\n", outcome);

    if (outcome == FP_OUTCOME_ERROR)
    {
      log_printf(LOG_LEVEL_ERROR, "-- Parse error: ");
      bytestring_dump(frameparser_get_error(fp));
      break;
    }
    else if (outcome == FP_OUTCOME_FRAME)
    {
      frame *f = frameparser_get_frame(fp);
      log_printf(LOG_LEVEL_INFO, "-- Completed frame: ");
      frame_dump(f);
      frame_free(f);
    }
  }

  close(fd);

  buffer_dump(b);

  buffer_free(b);
  frameparser_free(fp);
}
