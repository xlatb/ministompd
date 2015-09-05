#include <assert.h>
#include "ministompd.h"

queuebundle *queuebundle_new(void)
{
  queuebundle *qb = xmalloc(sizeof(queuebundle));

  qb->queues = hash_new(128);

  return qb;
}

void queuebundle_free(queuebundle *qb)
{
  // Free all contained queues
  queue *q;
  while ((q = hash_remove_any(qb->queues, NULL)))
    queue_free(q);

  hash_free(qb->queues);

  xfree(qb);
}

// Finds the queue object with the given name, creating it if it does not
//  exist yet.
queue *queuebundle_ensure_queue(queuebundle *qb, const bytestring *name)
{
  queue *q;

  if ((q = hash_get(qb->queues, name)))
    return q;

  q = queue_new(name, NULL);  // TODO: Supply the corresponding queueconfig

  bool added = hash_add(qb->queues, name, q);
  assert(added);

  return q;
}

// Subscribes a connection to a queue.
void queuebundle_subscribe(queuebundle *qb, connection *c, subscription *sub)
{
  // Queue should already exist within this bundle
  assert(hash_get(qb->queues, sub->queue->name) == sub->queue);

  bool subscribed = connection_subscribe(c, sub);
  assert(subscribed);

  framerouter_add_subscription(sub->queue->framerouter, sub);
}


