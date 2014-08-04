#include "queuetypes.h"

#ifndef MINISTOMPD_FRAMEROUTER_H
#define MINISTOMPD_FRAMEROUTER_H

#define FRAMEROUTER_DEFAULT_SIZE 8

framerouter *framerouter_new();
void framerouter_free(framerouter *fr);

#endif
