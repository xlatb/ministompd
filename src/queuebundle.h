#include "hash.h"
#include "queuetypes.h"

#ifndef MINISTOMPD_QUEUEBUNDLE_H
#define MINISTOMPD_QUEUEBUNDLE_H

typedef struct
{
  hash *queues;  // Map (name -> queue)
} queuebundle;

queuebundle *queuebundle_new(void);
void         queuebundle_free(queuebundle *qb);
queue       *queuebundle_ensure_queue(queuebundle *qb, const bytestring *name);
void         queuebundle_subscribe(queuebundle *qb, connection *c, subscription *sub);

#endif
