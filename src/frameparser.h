#include <stdlib.h>  // size_t
#include <stdint.h>  // uint8_t

#ifndef MINISTOMPD_FRAMEPARSER_H
#define MINISTOMPD_FRAMEPARSER_H

typedef enum
{
  FP_STATE_IDLE,     // No current frame
  FP_STATE_COMMAND,  // Reading frame command
  FP_STATE_HEADER,   // Reading frame header
  FP_STATE_BODY,     // Reading frame body
  FP_STATE_END,      // Reading NUL terminator
  FP_STATE_ERROR     // Halted due to error
} frameparser_state;

typedef enum
{
  FP_OUTCOME_WAITING,  // Waiting for more data
  FP_OUTCOME_FRAME,    // There is a complete frame ready to be picked up
  FP_OUTCOME_ERROR     // Unrecoverable error
} frameparser_outcome;

#define FP_LENGTH_UNKNOWN (-1)

typedef struct
{
  frameparser_state state;        // Current state
  int               length_left;  // Count of body bytes left to read to satisfy content-length header
  frame            *cur_frame;    // Current incomplete frame being parsed
  frame            *fin_frame;    // A finished frame that has not been picked up yet
  bytestring       *error;        // Error message, if any
} frameparser;

frameparser        *frameparser_new(void);
bytestring         *frameparser_get_error(frameparser *fp);
frame              *frameparser_get_frame(frameparser *fp);
frameparser_outcome frameparser_parse(frameparser *fp, buffer *b);
void                frameparser_free(frameparser *fp);

#endif
