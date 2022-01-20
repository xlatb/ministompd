#include "queuetypes.h"

#ifndef MINISTOMPD_FRAMEROUTER_H
#define MINISTOMPD_FRAMEROUTER_H

framerouter *framerouter_new();
void framerouter_free(framerouter *fr);
void framerouter_add_subscription(framerouter *fr, subscription *sub);
bool framerouter_remove_subscription(framerouter *fr, subscription *sub);
int  framerouter_subscription_count(framerouter *fr);
void framerouter_dispatch(framerouter *fr, frame *f, storage_handle sh);

#endif
