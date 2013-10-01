#include <string.h> // strcmp()

#include "ministompd.h"

struct frame_command_name_item {size_t length; const char *name;};
struct frame_command_name_item *frame_command_names = NULL;  // Created lazily

const char *frame_command_name(frame_command cmd)
{
  switch(cmd)
  {
  case CMD_STOMP:
    return "STOMP";
  case CMD_CONNECT:
    return "CONNECT";
  case CMD_CONNECTED:
    return "CONNECTED";
  case CMD_SEND:
    return "SEND";
  case CMD_SUBSCRIBE:
    return "SUBSCRIBE";
  case CMD_UNSUBSCRIBE:
    return "UNSUBSCRIBE";
  case CMD_BEGIN:
    return "BEGIN";
  case CMD_COMMIT:
    return "COMMIT";
  case CMD_ABORT:
    return "ABORT";
  case CMD_ACK:
    return "ACK";
  case CMD_NACK:
    return "NACK";
  case CMD_DISCONNECT:
    return "DISCONNECT";
  case CMD_MESSAGE:
    return "MESSAGE";
  case CMD_RECEIPT:
    return "RECEIPT";
  case CMD_ERROR:
    return "ERROR";
  default:
    abort();
  }
}

// Given a frame command name, returns the frame_command code. Returns
//  CMD_NONE on unknown command names.
frame_command frame_command_code(const uint8_t *name, size_t length)
{
  // If we haven't built the command name array yet, do it now
  if (frame_command_names == NULL)
  {
    frame_command_names = xmalloc(CMD_CODE_COUNT * sizeof(struct frame_command_name_item));

    for (int i = 0; i < CMD_CODE_COUNT; i++)
    {
      frame_command_names[i].name = frame_command_name(i);
      frame_command_names[i].length = strlen(frame_command_names[i].name);
    }
  }

  // Search for the command code
  for (int i = 0; i < CMD_CODE_COUNT; i++)
    if ((length == frame_command_names[i].length) && (memcmp(frame_command_names[i].name, name, length) == 0))
      return (frame_command) i;

  // Not found
  return CMD_NONE;
}

frame *frame_new(void)
{
  frame *f = xmalloc(sizeof(frame));

  f->command      = CMD_NONE;
  f->headerbundle = headerbundle_new();
  f->body         = NULL;

  return f;
}

frame_command frame_get_command(frame *f)
{
  return f->command;
}

void frame_set_command(frame *f, frame_command cmd)
{
  f->command = cmd;
}

headerbundle *frame_get_headerbundle(frame *f)
{
  return f->headerbundle;
}

// Returns a frame's body. May return NULL if there is no body yet.
bytestring *frame_get_body(frame *f)
{
  return f->body;
}

// Ensures that a frame has a body, creating a new blank body if necessary.
//  Returns a pointer to the body.
bytestring *frame_ensure_body(frame *f)
{
  if (!f->body)
    f->body = bytestring_new(0);

  return f->body;
}

void frame_dump(frame *f)
{
  printf("== frame ==\n");
  printf("%s\n", frame_command_name(f->command));
  headerbundle_dump(f->headerbundle);
  bytestring_dump(f->body);
}

void frame_free(frame *f)
{
  headerbundle_free(f->headerbundle);

  if (f->body)
    bytestring_free(f->body);

  xfree(f);
}

