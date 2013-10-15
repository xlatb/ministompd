#include "queuetypes.h"
#include "storage/memory.h"

#ifndef MINISTOMPD_STORAGE_H
#define MINISTOMPD_STORAGE_H

typedef void storage_func_init(storage *s);
typedef void storage_func_deinit(storage *s);
typedef bool storage_func_enqueue(storage *s, frame *f);

struct storage_funcs
{
  storage_func_init    *init;
  storage_func_deinit  *deinit;
  storage_func_enqueue *enqueue;
};

storage *storage_new(storage_type type, queue *q);
void     storage_free(storage *s);
bool     storage_enqueue(storage *s, frame *f);

#endif
