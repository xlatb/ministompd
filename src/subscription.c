#include "ministompd.h"

// Creates a subscription for the given queue.
// Does not take ownership of the queue.
subscription *subscription_new(queue *queue, bytestring *subid, sub_ack_type ack_type)
{
  subscription *sub = xmalloc(sizeof(subscription));

  sub->queue     = queue;
  sub->subid     = subid;
  sub->ack_type  = ack_type;
  sub->last_qlid = 0;

  return sub;
}

void subscription_free(subscription *sub)
{
  bytestring_free(sub->subid);
  xfree(sub);
}

