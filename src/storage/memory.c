#include "../ministompd.h"

static bool storage_memory_ensure_size(storage *s, int size)
{
  storage_memory *mem = s->u.memory;

  if (mem->size >= size)
    return true;  // Already at the given size or larger

  if (size > s->queue->config->size_max)
    return false;  // Can't grow beyond max size

  // Find new size
  while (mem->size < size)
    mem->size <<= 1;

  // Cap at maximum size, if needed
  if (mem->size > s->queue->config->size_max)
    mem->size = s->queue->config->size_max;

  // Resize the slot array
  mem->slots = xrealloc(mem->slots, sizeof(storage_memory_slot) * mem->size);

  return true;
}

void storage_memory_init(storage *s)
{
  storage_memory *mem = xmalloc(sizeof(storage_memory));

  mem->next_qlid = 0;
  mem->size      = 16;  // Reasonable starting size?
  mem->length    = 0;
  mem->slots     = xmalloc(sizeof(storage_memory_slot) * mem->size);

  s->u.memory = mem;
}

void storage_memory_deinit(storage *s)
{
  storage_memory *mem = s->u.memory;

  xfree(mem->slots);
  xfree(mem);

  s->u.memory = NULL;
}

bool storage_memory_enqueue(storage *s, frame *f)
{
  storage_memory *mem = s->u.memory;

  // Check if more space is needed
  if (mem->length == mem->size)
  {
    if (!storage_memory_ensure_size(s, mem->length + 1))
    {
      // TODO: Carry out the full_policy
      log_printf(LOG_LEVEL_ERROR, "Queue out of storage space.\n");
      exit(1);
    }
  }

  // Add to queue
  int i = mem->length++;
  mem->slots[i].qlid        = mem->next_qlid++;
  mem->slots[i].rejectcount = 0;
  mem->slots[i].frame       = f;

  return true;
}
