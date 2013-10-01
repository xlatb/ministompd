#include <stdio.h>
#include <stdlib.h>  // malloc(), etc
#include "alloc.h"

// A malloc() which prints an error and dies if the request cannot be satisfied.
void *xmalloc(size_t size)
{
  void *ptr = malloc(size);
  if (ptr)
    return ptr;

  fprintf(stderr, "xmalloc(): Could not allocate %zd bytes.\n", size);
  abort();
}

// A realloc() which prints an error and dies if the request cannot be satisfied.
void *xrealloc(void *ptr, size_t size)
{
  void *newptr = realloc(ptr, size);
  if (newptr)
    return newptr;

  fprintf(stderr, "xrealloc(): Could not allocate %zd bytes\n", size);
  abort();
}

// Wrapper for free().
void xfree(void *ptr)
{
  free(ptr);
}
