#include "ministompd.h"

// Creates a new queue. Takes ownership of queue name, but not config.
queue *queue_new(bytestring *name, const queueconfig *config)
{
  queue *q = xmalloc(sizeof(queue));

  q->name        = name;
  q->storage     = storage_new(config->storage_type, q);
  q->framerouter = framerouter_new();
  q->config      = config;

  return q;
}

void queue_free(queue *q)
{
  bytestring_free(q->name);
  storage_free(q->storage);
  framerouter_free(q->framerouter);

  xfree(q);
}

bool queue_enqueue(queue *q, frame *f)
{
  return storage_enqueue(q->storage, f);
}
