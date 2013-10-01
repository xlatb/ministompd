#include <sys/types.h>  // open()
#include <sys/stat.h>   // open()
#include <fcntl.h>      // open()
#include <sys/select.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>  // STDIN_FILENO
#include "ministompd.h"

listener *l;

void loop(void);
void parse_file(char *filename);
void handle_connection(connection *c);

int main(int argc, char *argv[])
{
  // Create a new listener
  l = listener_new();
  if (!listener_set_address(l, "::1", 61613))
  {
    printf("Couldn't set listening address.\n");
    exit(1);
  }
  else if (!listener_listen(l))
  {
    printf("Couldn't listen on socket.\n");
    exit(1);
  }

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
    printf("Select returned: %d\n", count);

    // Give up if the select() didn't work
    if (count < 0)
    {
      perror("select()");
      exit(1);
    }

    // If nothing happened, keep waiting
    if (count == 0)
      continue;

    // Check for new connections
    connection *c = listener_accept_connection(l, &readfds);
    if (c != NULL)
    {
      connectionbundle_add_connection(cb, c);
    }

    // Check for activity on existing connections
    cb_iter iter = connectionbundle_iter_new(cb);
    while ((c = connectionbundle_get_next_active_connection(cb, &iter, &readfds, &writefds)))
    {
      handle_connection(c);
    }
  }
}

void handle_connection(connection *c)
{
  printf("Connection %p is interesting.\n", c);
  connection_dump(c);

  connection_pump(c);
}

void parse_file(char *filename)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    perror("open()");
    exit(1);
  }

  frameparser *fp = frameparser_new();

  buffer *b = buffer_new(4096);

  while (buffer_in_from_fd(b, fd, 4096) > 0)
  {
    printf("Read loop...\n");

    frameparser_outcome outcome = frameparser_parse(fp, b);
    printf("Parse: %d\n", outcome);

    if (outcome == FP_OUTCOME_ERROR)
    {
      printf("-- Parse error: ");
      bytestring_dump(frameparser_get_error(fp));
      break;
    }
    else if (outcome == FP_OUTCOME_FRAME)
    {
      frame *f = frameparser_get_frame(fp);
      printf("-- Completed frame: ");
      frame_dump(f);
      frame_free(f);
    }
  }

  close(fd);

  buffer_dump(b);

  buffer_free(b);
  frameparser_free(fp);
}
