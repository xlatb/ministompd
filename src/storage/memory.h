#include "../queuetypes.h"

#ifndef MINISTOMPD_STORAGE_MEMORY_H
#define MINISTOMPD_STORAGE_MEMORY_H

typedef struct
{
  queue_local_id qlid;
  int            rejectcount;  // Number of times the frame has been rejected by a consumer
  frame         *frame;
} storage_memory_slot;

struct storage_memory
{
  queue_local_id       next_qlid;  // The next queue-local id to be assigned
  int                  size;       // Count of frame slots allocated
  int                  length;     // Count of frame slots in use
  storage_memory_slot *slots;      // Array of slots
};
typedef struct storage_memory storage_memory;

void storage_memory_init(storage *s);
void storage_memory_deinit(storage *s);
bool storage_memory_enqueue(storage *s, frame *f);

#endif
