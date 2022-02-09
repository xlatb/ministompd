#include <assert.h>  // assert()
#include <string.h>  // memmove()

#include "ministompd.h"
#include "list.h"

static void list_ensure_size(list *l, int size)
{
  if (size <= l->size)
    return;  // Already at least the given size

  // List is too small, grow it
  while (l->size < size)
    l->size <<= 1;

  l->items = xrealloc(l->items, l->size * sizeof(void *));

  return;
}

void list_init(list *l, int size)
{
  // Size must be positive
  if (size < 1)
    size = 1;

  l->size   = size;
  l->length = 0;
  l->items  = xmalloc(sizeof(void *) * size);
}

list *list_new(int size)
{
  list *l = xmalloc(sizeof(list));

  list_init(l, size);

  return l;
}

int list_search(const list *l, void *ptr)
{
  for (int i = 0; i < l->length; i++)
  {
    if (l->items[i] == ptr)
      return i;
  }

  return -1;
}

void list_remove(list *l, int i)
{
  assert(i >= 0);
  assert(i < l->length);

  if (i < (l->length - 1))
    memmove(l->items + i, l->items + i + 1, sizeof(void *) * (l->length - i - 1));

  l->length--;
}

void list_push(list *l, void *item)
{
  int len = l->length + 1;
  list_ensure_size(l, len);

  l->length = len;
  l->items[len - 1] = item;
}

void *list_pop(list *l)
{
  if (l->length == 0)
    return NULL;

  void *item = l->items[l->length - 1];
  l->length--;

  return item;
}

void *list_shift(list *l)
{
  if (l->length == 0)
    return NULL;

  void *item = l->items[0];

  l->length--;
  if (l->length)
    memmove(l->items, l->items + 1, l->length * sizeof(*l->items));

  return item;
}

void list_free(list *l)
{
  xfree(l->items);
  xfree(l);
}
