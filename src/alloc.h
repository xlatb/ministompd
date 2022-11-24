#include <stdlib.h>  // size_t

#ifndef MINISTOMPD_ALLOC_H
#define MINISTOMPD_ALLOC_H

void *xmalloc(size_t size);
void *xmalloc_zero(size_t size);
void *xrealloc(void *ptr, size_t size);
void  xfree(void *ptr);

#endif
