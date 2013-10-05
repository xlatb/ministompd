#include <string.h>  // strlen()
#include "ministompd.h"

// Returns the number of bytes the given string would take up after escaping
//  octets that have special meaning in headers (0x0A, 0x0D, 0x3A, 0x5C). If
//  the result is the same as the bytestring length, the bytestring does not
//  require escaping.
static size_t header_bytestring_escaped_length(const bytestring *bs)
{
  size_t hlen = bytestring_get_length(bs);
  size_t elen = 0;

  const uint8_t *bytes = bytestring_get_bytes(bs);
  for (int i = 0; i < hlen; i++)
  {
    switch (bytes[i])
    {
    case 0x0A:
    case 0x0D:
    case 0x3A:
    case 0x5C:
      elen++;
    }
    elen++;
  }

  return elen;
}

// Performs header octet escaping on the 'in' bytestring, appending the result
//  to the 'out' bytestring.
static void escape_header_bytestring(bytestring *out, const bytestring *in)
{
  size_t ilen = bytestring_get_length(in);

  const uint8_t *bytes = bytestring_get_bytes(in);
  for (int i = 0; i < ilen; i++)
  {
    switch (bytes[i])
    {
    case 0x0A:  // LF
      bytestring_append_byte(out, '\\');
      bytestring_append_byte(out, 'n');
      break;
    case 0x0D:  // CR
      bytestring_append_byte(out, '\\');
      bytestring_append_byte(out, 'r');
      break;
    case 0x3A:  // Colon
      bytestring_append_byte(out, '\\');
      bytestring_append_byte(out, 'c');
      break;
    case 0x5C:  // Backslash
      bytestring_append_byte(out, '\\');
      bytestring_append_byte(out, '\\');
      break;
    default:
      bytestring_append_byte(out, bytes[i]);
      break;
    }
  }

  return;
}

frameserializer *frameserializer_new(void)
{
  frameserializer *fs = xmalloc(sizeof(frameserializer));

  fs->state                  = FS_STATE_IDLE;
  fs->nextqid                = 1;
  fs->work_queue_size        = FS_QUEUE_SIZE;
  fs->work_queue_length      = 0;
  fs->work_queue             = xmalloc(sizeof(fs_work_item) * fs->work_queue_size);
  fs->completed_queue_size   = FS_QUEUE_SIZE;
  fs->completed_queue_length = 0;
  fs->completed_queue        = xmalloc(sizeof(fs_completed_item) * fs->completed_queue_size);

  return fs;
}

void frameserializer_free(frameserializer *fs)
{
  // TODO: Free any constituent frames
  xfree(fs->work_queue);
  xfree(fs->completed_queue);
  xfree(fs);
}

// Adds the given frame to the work queue. The frameserializer takes ownership
//  of the frame. Returns the qid, or zero if the frame could not be queued.
int frameserializer_enqueue_frame(frameserializer *fs, frame *f)
{
  // Bounds check
  if (fs->work_queue_length >= fs->work_queue_size)
    return 0;  // No room in work queue

  // Fill in new item
  fs_work_item *item = &fs->work_queue[fs->work_queue_length];
  item->frame        = f;
  item->qid          = fs->nextqid;
  item->state        = FS_WORK_STATE_COMMAND;
  item->header_index = 0;
  item->body_index   = 0;

  // Housekeeping
  fs->work_queue_length++;
  fs->nextqid++;

  return item->qid;
}

// Moves the head frame from the work queue to the tail of the completed
//  queue, and marks it with the given state. Returns the qid, or zero if no
//  frame was moved.
static int frameserializer_complete_frame(frameserializer *fs, fs_completed_item_state state)
{
  if (fs->work_queue_length < 1)
    return 0;  // No frame at the head of the work queue
  else if (fs->completed_queue_length >= fs->completed_queue_size)
    return 0;  // No room in completed queue

  // Get head work item
  fs_work_item *witem = &fs->work_queue[0];

  // Add new tail completed item
  fs_completed_item *citem = &fs->completed_queue[fs->completed_queue_length];
  citem->frame = witem->frame;
  citem->qid   = witem->qid;
  citem->state = state;
  fs->completed_queue_length++;

  // Remove old work item
  fs->work_queue_length--;
  memmove(fs->work_queue, fs->work_queue + 1, sizeof(fs->work_queue[0]) * fs->work_queue_length);

  return citem->qid;
}

// Serializes a frame command. Returns true iff progress was made.
static bool frameserializer_serialize_command(frameserializer *fs, buffer *b)
{
  fs_work_item *item = &fs->work_queue[0];

  const char *name = frame_command_name(item->frame->command);
  size_t namelen = strlen(name);

  if (buffer_input_bytes(b, (const uint8_t *) name, namelen) != namelen)
    abort();  // Short write

  if (buffer_input_byte(b, '\n') != 1)
    abort();  // Short write

  item->state = FS_WORK_STATE_HEADERS;

  return true;
}

// Serializes a frame header. Returns true iff progress was made.
static bool frameserializer_serialize_header(frameserializer *fs, buffer *b)
{
  fs_work_item *item = &fs->work_queue[0];

  headerbundle *hb = frame_get_headerbundle(item->frame);

  // All headers done? Add terminating newline
  if (item->header_index >= hb->count)
  {
    // Add extra linefeed to terminate headers
    if (buffer_input_byte(b, '\n') != 1)
      abort();  // Short write

    item->state = FS_WORK_STATE_BODY;
    return true;
  }

  // Get header data
  const bytestring *key;
  const bytestring *val;
  if (!headerbundle_get_header(hb, item->header_index, &key, &val))
    abort();  // Should never happen

  // Find header data escaped lengths
  size_t ekeylen = header_bytestring_escaped_length(key);
  size_t evallen = header_bytestring_escaped_length(val);

  // TODO: Ensure buffer space before allocating memory

  // Escape header key, if needed
  bytestring *ekey = NULL;
  if (ekeylen != bytestring_get_length(key))
  {
    ekey = bytestring_new(ekeylen);
    escape_header_bytestring(ekey, key);
  }

  // Escape header value, if needed
  bytestring *eval = NULL;
  if (evallen != bytestring_get_length(val))
  {
    eval = bytestring_new(evallen);
    escape_header_bytestring(eval, val);
  }

  // Write header data to the buffer
  if (buffer_input_bytestring(b, ekey ? ekey : key) != ekeylen)
    abort();  // Short write
  if (buffer_input_byte(b, ':') != 1)
    abort();  // Short write
  if (buffer_input_bytestring(b, eval ? eval : val) != evallen)
    abort();  // Short write
  if (buffer_input_byte(b, '\n') != 1)
    abort();  // Short write

  // Clean up escaped headers, if any
  if (ekey)
    bytestring_free(ekey);
  if (eval)
    bytestring_free(eval);

  // All done with this header
  item->header_index++;

  return true;
}

// Serializes a frame body. Returns true iff progress was made.
static bool frameserializer_serialize_body(frameserializer *fs, buffer *b)
{
  fs_work_item *item = &fs->work_queue[0];
  const bytestring *body = item->frame->body;

  int writecount = 0;

  // If we haven't sent the entire body yet, push out as many bytes as possible
  size_t bodylen = body ? bytestring_get_length(body) : 0;
  if (item->body_index < bodylen)
  {
    int count = buffer_input_bytestring_slice(b, body, item->body_index, bodylen - item->body_index);
    item->body_index += count;
    writecount += count;
  }

  // If the body is done, and there's at least one slot in the completed
  //  queue, send terminating NUL byte and move the frame to that queue.
  if ((item->body_index >= bodylen) && (fs->completed_queue_length < fs->completed_queue_size))
  {
    if (buffer_input_byte(b, '\x00') != 1)
      abort();  // Short write

    frameserializer_complete_frame(fs, FS_COMPLETED_STATE_SUCCESS);
    writecount++;
  }

  return (writecount > 0);
}

// Serializes next step of a frame. Returns true iff progress was made.
static bool frameserializer_serialize_internal(frameserializer *fs, buffer *b)
{
  switch (fs->work_queue[0].state)
  {
  case FS_WORK_STATE_COMMAND:
    return frameserializer_serialize_command(fs, b);
    break;
  case FS_WORK_STATE_HEADERS:
    return frameserializer_serialize_header(fs, b);
    break;
  case FS_WORK_STATE_BODY:
    return frameserializer_serialize_body(fs, b);
    break;
  }

  abort();  // Should never happen
}

// Serialize as much as possible into the given buffer.
void frameserializer_serialize(frameserializer *fs, buffer *b)
{
  bool done = false;

  while ((!done) && (fs->work_queue_length > 0))
  {
    if (!frameserializer_serialize_internal(fs, b))
      done = true;
  }

  return;
}
