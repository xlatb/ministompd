#include "ministompd.h"

static struct storage_funcs funcs[] =
{
  {init: &storage_memory_init, deinit: &storage_memory_deinit, enqueue: &storage_memory_enqueue}
};

// Does not take ownership of queue.
storage *storage_new(storage_type type, queue *q)
{
  storage *s = xmalloc(sizeof(storage));

  s->type   = type;
  s->queue  = q;
  (*funcs[type].init)(s);

  return s;
}

void storage_free(storage *s)
{
  (*funcs[s->type].deinit)(s);

  xfree(s);
}

bool storage_enqueue(storage *s, frame *f)
{
  return (*funcs[s->type].enqueue)(s, f);
}

