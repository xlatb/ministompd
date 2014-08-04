#include "queuetypes.h"

#ifndef MINISTOMPD_SUBSCRIPTION_H
#define MINISTOMPD_SUBSCRIPTION_H

subscription *subscription_new(queue *queue, bytestring *subid, sub_ack_type ack_type);
void subscription_free(subscription *sub);

#endif
