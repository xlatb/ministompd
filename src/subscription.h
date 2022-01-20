#include "queuetypes.h"

#ifndef MINISTOMPD_SUBSCRIPTION_H
#define MINISTOMPD_SUBSCRIPTION_H

subscription *subscription_new(queue *queue, connection *connection, const bytestring *client_id, const bytestring *server_id, sub_ack_type ack_type);
void          subscription_deliver(subscription *s, frame *f);
void          subscription_dump(subscription *s);
void          subscription_free(subscription *sub);

#endif
