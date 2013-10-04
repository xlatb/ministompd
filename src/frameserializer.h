#ifndef MINISTOMPD_SERIALIZER_H
#define MINISTOMPD_SERIALIZER_H

typedef enum
{
  FS_WORK_STATE_COMMAND,
  FS_WORK_STATE_HEADERS,
  FS_WORK_STATE_BODY
} fs_work_item_state;

typedef struct
{
  frame             *frame;         // Frame to be serialized
  int                qid;           // Queue id for this item
  fs_work_item_state state;         // State of this work item
  int                header_index;  // Next header to send
  int                body_index;    // Next body byte to send
} fs_work_item;

typedef enum
{
  FS_COMPLETED_STATE_SUCCESS,
  FS_COMPLETED_STATE_FAILURE
} fs_completed_item_state;

typedef struct
{
  frame                  *frame;  // Frame
  int                     qid;    // Queue id for this item
  fs_completed_item_state state;  // State of this completed item
} fs_completed_item;

#define FS_QUEUE_SIZE 16

typedef enum
{
  FS_STATE_IDLE,  // No current frame
  FS_STATE_BUSY   // Busy
} frameserializer_state;

typedef struct
{
  frameserializer_state state;    // Current state
  int                   nextqid;  // Next queue id to be assigned

  fs_work_item         *work_queue;         // Array of work items
  int                   work_queue_size;    // Number of slots in work queue
  int                   work_queue_length;  // Number of contiguous slots filled

  fs_completed_item    *completed_queue;         // Array of completed items
  int                   completed_queue_size;    // Number of slots in completed queue
  int                   completed_queue_length;  // Number of contiguous slots filled
} frameserializer;

frameserializer *frameserializer_new(void);
void             frameserializer_free(frameserializer *fs);
int              frameserializer_enqueue_frame(frameserializer *fs, frame *f);
void             frameserializer_serialize(frameserializer *fs, buffer *b);

#endif
