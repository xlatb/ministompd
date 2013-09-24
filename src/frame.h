#include "headerbundle.h"

#ifndef MINISTOMPD_FRAME_H
#define MINISTOMPD_FRAME_H

typedef enum
{
  CMD_STOMP,
  CMD_CONNECT,
  CMD_CONNECTED,

  CMD_SEND,
  CMD_SUBSCRIBE,
  CMD_UNSUBSCRIBE,
  CMD_BEGIN,
  CMD_COMMIT,
  CMD_ABORT,
  CMD_ACK,
  CMD_NACK,
  CMD_DISCONNECT,

  CMD_MESSAGE,
  CMD_RECEIPT,
  CMD_ERROR,

  CMD_CODE_COUNT  // Number of command codes defined
} frame_command;

#define CMD_NONE (-1)

typedef struct
{
  frame_command command;
  headerbundle *headerbundle;
  bytestring   *body;  // May be NULL if no body
} frame;

const char   *frame_command_name(frame_command cmd);
frame_command frame_command_code(const uint8_t *name, size_t length);

frame        *frame_new(void);
frame_command frame_get_command(frame *f);
void          frame_set_command(frame *f, frame_command cmd);
headerbundle *frame_get_headerbundle(frame *f);
bytestring   *frame_get_body(frame *f);
bytestring   *frame_ensure_body(frame *f);
void          frame_dump(frame *f);
void          frame_free(frame *f);

#endif
