#include "queuetypes.h"
#include "queueconfig.h"

#ifndef MINISTOMPD_QUEUE_H
#define MINISTOMPD_QUEUE_H

queue *queue_new(bytestring *name, const queueconfig *config);
void   queue_free(queue *q);
bool   queue_enqueue(queue *q, frame *f);

#endif
