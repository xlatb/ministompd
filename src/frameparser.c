#include <stdbool.h>
#include "ministompd.h"

// Unescapes a bytestring according to the rules used for headers:
//  "\r" => CR, "\n" => LF, "\c" => ":", "\\" => "\"
// If the input bytestring is well-formed, it is consumed and a new bytestring
//  is returned. If the input bytestring is malformed, it is not consumed and
//  NULL is returned.
static bytestring *unescape_header_bytestring(bytestring *in)
{
  bytestring *out = NULL;
  bool success = true;
  size_t inlen = bytestring_get_length(in);
  int pos = 0;

  // Handle trivial case of zero-length input
  if (inlen == 0)
    return in;

  while (pos < inlen)
  {
    // Find next backslash
    int bspos = bytestring_find_byte(in, '\\', pos);
    if (bspos == -1)
    {
      // No backslash, can take the rest of the input verbatim
      if (!out)
        return in;  // No transforms needed, so just return the input string

      // Create output string, if needed
      if (!out)
        out = bytestring_new(bytestring_get_length(in));

      // Grab rest of input string
      size_t count = inlen - pos;
      bytestring_append_bytestring(out, in, pos, count);
      pos += count;
    }
    else
    {
      // There was a backslash, so we'll need to do some transforming

      // Create output string, if needed
      if (!out)
        out = bytestring_new(bytestring_get_length(in));

      // Grab all characters before the backslash, if any
      if (bspos > 0)
      {
        // Copy everything before the backslash
        size_t count = bspos - pos;
        bytestring_append_bytestring(out, in, pos, count);
        pos += count;
      }

      // Process the character following the backslash
      if (bytestring_get_length(in) == (bspos + 1))
      {
        success = false;  // There is no following character
        break;
      }
      else
      {
        uint8_t c = bytestring_get_bytes(in)[bspos + 1];
        if (c == '\\')
          bytestring_append_byte(out, '\\');
        else if (c == 'r')
          bytestring_append_byte(out, '\x0D');  // CR
        else if (c == 'n')
          bytestring_append_byte(out, '\x0A');  // LF
        else if (c == 'c')
          bytestring_append_byte(out, ':');
        else
        {
          success = false;  // Invalid escape sequence
          break;
        }

        pos += 2;
      }
    }
  }

  // Handle errors
  if (!success)
  {
    if (out)
      bytestring_free(out);
    return NULL;
  }

  // All done
  bytestring_free(in);
  return out;
}

frameparser *frameparser_new(void)
{
  frameparser *fp = xmalloc(sizeof(frameparser));

  fp->state       = FP_STATE_IDLE;
  fp->length_left = FP_LENGTH_UNKNOWN;
  fp->cur_frame   = NULL;
  fp->fin_frame   = NULL;
  fp->error       = NULL;

  return fp;
}

// Record an error
void frameparser_set_error(frameparser *fp, char *fmt, ...)
{
  fp->state = FP_STATE_ERROR;

  // Bail out if there is already an error. We only want to record the first one.
  if (fp->error)
    return;

  va_list args;

  va_start(args, fmt);
  fp->error = bytestring_append_vprintf(bytestring_new(0), fmt, args);
  va_end(args);

  return;
}

const bytestring *frameparser_get_error(frameparser *fp)
{
  return fp->error;
}

// Returns the next finished frame to the caller. Caller takes ownership of
//  the frame.
frame *frameparser_get_frame(frameparser *fp)
{
  frame *f = fp->fin_frame;  // Could be NULL, if no finished frame is waiting
  fp->fin_frame = NULL;
  return f;
}

// Called when we are done reading all the headers.
void frameparser_parse_headers_complete(frameparser *fp)
{
  // Some frame types have no bodies
  frame_command cmd = frame_get_command(fp->cur_frame);
  if ((cmd != CMD_SEND) && (cmd != CMD_MESSAGE) && (cmd != CMD_ERROR))
  {
    fp->state = FP_STATE_END;
    return;
  }

  // There should be a body following, so check for a content-length header
  const bytestring *bs = headerbundle_get_header_value_by_str(fp->cur_frame->headerbundle, "content-length");
  if (bs == NULL)
    fp->length_left = FP_LENGTH_UNKNOWN;  // No content-length, will have to read until NUL
  else
  {
    int end;
    long value;
    if (!bytestring_strtol(bs, 0, &end, &value, 10) || (bytestring_get_length(bs) != end))
    {
      frameparser_set_error(fp, "Contents of 'content-length' header is not a valid number");
      return;
    }
    else if ((value < 0) || (value > LIMIT_FRAME_BODY_LEN))
    {
      frameparser_set_error(fp, "Value of 'content-length' header is out of range");
      return;
    }

    printf("Content-length is: %ld\n", value);
    fp->length_left = value;
  }

  // Transition to body state
  fp->state = FP_STATE_BODY;

  return;
}

// Parses keepalive linefeeds. Returns true iff progress was made.
bool frameparser_parse_idle(frameparser *fp, buffer *b)
{
  printf("frameparser_parse_idle\n");
  // Valid input in this state is a CR/LF pair, or bare LF

  uint8_t byte = buffer_get_byte(b, 0);
  if (byte == '\x0D')
  {
    // Should be CR/LF pair. Make sure we have at least two bytes
    size_t len = buffer_get_length(b);
    if (len < 2)
      return false;  // No progress

    // Make sure second byte is LF
    byte = buffer_get_byte(b, 1);
    if (byte != '\x0A')
    {
      frameparser_set_error(fp, "Expected 0x0A after 0x0D, got 0x%02X", byte);
      return false;  // No progress
    }

    // Eat the CR/LF pair
    buffer_consume(b, 2);
    return true;
  }
  else if (byte == '\x0A')
  {
    // Bare LF
    buffer_consume(b, 1);
    return true;
  }

  // Something else? Must be the start of a frame
  fp->state = FP_STATE_COMMAND;
  return true;
}

// Parses a frame's command. Returns true iff progress was made.
bool frameparser_parse_command(frameparser *fp, buffer *b)
{
  printf("frameparser_parse_command\n");
  // Valid input in this state is a string followed by CR/LF or LF, which matches a known frame command

  // Try to find LF line terminator
  int lfpos = buffer_find_byte(b, '\x0A');
  if (lfpos < 0)  // No LF yet?
  {
    if (buffer_get_length(b) > LIMIT_FRAME_CMD_LINE_LEN)
      frameparser_set_error(fp, "Line length limit exceeded waiting for command");

    return false;  // No progress
  }
  else if (lfpos == 0)
    abort();  // Should never happen in this state

  // Figure out number of bytes in command string
  int len = lfpos;
  if (buffer_get_byte(b, len - 1) == '\x0D')
    len--;

  // Extract the command into a new bytestring
  bytestring *bs = bytestring_new(len);
  buffer_append_bytestring(b, bs, 0, len);
  bytestring_dump(bs);

  // Consume the current line
  buffer_consume(b, lfpos + 1);

  // Find the command code for this command
  frame_command cmd = frame_command_code(bytestring_get_bytes(bs), bytestring_get_length(bs));

  // Clean up the bytestring
  bytestring_free(bs);

  // Make sure command is valid
  if (cmd == CMD_NONE)
  {
    frameparser_set_error(fp, "Unknown command");
    return false;
  }

  // Set up a new frame structure to hold parsed data
  if (!fp->cur_frame)
    fp->cur_frame = frame_new();

  // Store parsed data
  frame_set_command(fp->cur_frame, cmd);

  // Got a valid command, so headers should be next
  fp->state = FP_STATE_HEADER;
  return true;
}

// Parses a header line. Returns true iff progress was made.
bool frameparser_parse_header(frameparser *fp, buffer *b)
{
  printf("frameparser_parse_header\n");
  // Valid input in this state is either:
  // 1. CR/LF or LF alone, denoting the end of headers
  // 2. A key name, followed by a colon, followed by a value name, terminated with CR/LF or LF

  // Try to find LF line terminator
  int lfpos = buffer_find_byte(b, '\x0A');
  if (lfpos < 0)  // No LF yet?
  {
    if (buffer_get_length(b) > LIMIT_FRAME_HEADER_LINE_LEN)
      frameparser_set_error(fp, "Line length limit exceeded waiting for header");

    return false;  // No progress
  }
  else if (lfpos == 0)  // LF alone
  {
    buffer_consume(b, 1);
    frameparser_parse_headers_complete(fp);
    return true;
  }
  else if ((lfpos == 1) && (buffer_get_byte(b, 0) == '\x0D'))  // CR/LF alone
  {
    buffer_consume(b, 2);
    frameparser_parse_headers_complete(fp);
    return true;
  }

  // Figure out number of bytes in the line
  int len = lfpos;
  if (buffer_get_byte(b, len - 1) == '\x0D')
    len--;

  // Find the colon delimiter
  int colonpos = buffer_find_byte_within(b, ':', 0, len);
  if (colonpos < 0)  // No colon?
  {
    frameparser_set_error(fp, "Expected colon delimiter on header line");
    return false;
  }
  else if (colonpos == 0)  // Starts with colon?
  {
    frameparser_set_error(fp, "Header name has zero length");
    return false;
  }

  // Extract the key
  bytestring *key = bytestring_new(colonpos);
  buffer_append_bytestring(b, key, 0, colonpos);

  // Extract the value
  bytestring *val = bytestring_new(len - colonpos - 1);
  buffer_append_bytestring(b, val, colonpos + 1, len - colonpos - 1);

  // Consume the current line
  buffer_consume(b, lfpos + 1);

  // Unescape the key and value if needed
  frame_command cmd = frame_get_command(fp->cur_frame);
  if ((cmd != CMD_CONNECT) && (cmd != CMD_CONNECTED))
  {
    key = unescape_header_bytestring(key);
    val = unescape_header_bytestring(val);
  }

  bytestring_dump(key);
  bytestring_dump(val);

  headerbundle *hb = frame_get_headerbundle(fp->cur_frame);
  headerbundle_append_header(hb, key, val);  // The bundle takes ownership of key and val

  return true;
}

// Parses body. Returns true iff progress was made.
bool frameparser_parse_body(frameparser *fp, buffer *b)
{
  printf("frameparser_parse_body\n");

  if (fp->length_left == FP_LENGTH_UNKNOWN)
  {
    // Look for NUL terminator
    int nulpos = buffer_find_byte(b, '\x00');
    if (nulpos < 0)
    {
      // No NUL, so grab everything
      int count = buffer_get_length(b);
      bytestring *body = frame_ensure_body(fp->cur_frame);
      buffer_append_bytestring(b, body, 0, count);
      buffer_consume(b, count);
      return true;
    }
    else if (nulpos == 0)
    {
      // NUL as next character, so the frame is complete
      fp->state = FP_STATE_END;
      return true;
    }
    else
    {
      // NUL after some other bytes, so grab them and the frame is complete
      bytestring *body = frame_ensure_body(fp->cur_frame);
      buffer_append_bytestring(b, body, 0, nulpos);
      buffer_consume(b, nulpos);
      fp->state = FP_STATE_END;
      return true;
    }
  }
  else
  {
    // Remaining length is known. Figure out how many bytes to grab.
    int count = buffer_get_length(b);
    if (fp->length_left < count)
      count = fp->length_left;

    // Grab the bytes
    bytestring *body = frame_ensure_body(fp->cur_frame);
    buffer_append_bytestring(b, body, 0, count);
    buffer_consume(b, count);
    fp->length_left -= count;

    // If we satisfied the expected length then the frame is complete
    if (fp->length_left == 0)
      fp->state = FP_STATE_END;

    return true;
  }
}

// Parses trailing NUL character. Returns true iff progress was made.
bool frameparser_parse_end(frameparser *fp, buffer *b)
{
  // Valid input in this state is simply a single NUL character
  printf("frameparser_parse_end\n");

  // If there is already a parsed frame that hasn't been picked up yet, we
  //  don't want to overwrite it, so refrain from finishing this frame.
  if (fp->fin_frame)
    return false;  // No progress

  // Check for the NUL
  uint8_t byte = buffer_get_byte(b, 0);
  if (byte != '\x00')
  {
    frameparser_set_error(fp, "Expected trailing NUL at end of frame");
    return false;
  }

  // Consume it
  buffer_consume(b, 1);

  // All done with the current frame
  fp->fin_frame = fp->cur_frame;
  fp->cur_frame = NULL;
  fp->state = FP_STATE_IDLE;

  return true;
}

// Parses one step. Returns true iff progress was made.
bool frameparser_parse_internal(frameparser *fp, buffer *b)
{
  switch (fp->state)
  {
  case FP_STATE_ERROR:
    return false;  // We have an error; refuse to parse more
    break;
  case FP_STATE_IDLE:
    return frameparser_parse_idle(fp, b);
    break;
  case FP_STATE_COMMAND:
    return frameparser_parse_command(fp, b);
    break;
  case FP_STATE_HEADER:
    return frameparser_parse_header(fp, b);
    break;
  case FP_STATE_BODY:
    return frameparser_parse_body(fp, b);
    break;
  case FP_STATE_END:
    return frameparser_parse_end(fp, b);;
    break;
  default:
    // Should never happen
    abort();
    break;
  }
}

frameparser_outcome frameparser_parse(frameparser *fp, buffer *b)
{
  // Parse as much as we can from the buffer
  bool done = false;
  while ((!done) && (buffer_get_length(b) > 0))
  {
    if (!frameparser_parse_internal(fp, b))
      done = true;
  }

  // Can't make any more parsing progress for now. Might as well compact the buffer.
  buffer_compact(b);

  if (fp->state == FP_STATE_ERROR)
    return FP_OUTCOME_ERROR;
  else if (fp->fin_frame)
    return FP_OUTCOME_FRAME;

  return FP_OUTCOME_WAITING;
}

void frameparser_free(frameparser *fp)
{
  if (fp->cur_frame)
    xfree(fp->cur_frame);

  if (fp->fin_frame)
    xfree(fp->fin_frame);

  if (fp->error)
    bytestring_free(fp->error);

  xfree(fp);
}
