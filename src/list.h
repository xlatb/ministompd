#ifndef MINISTOMPD_LIST_H
#define MINISTOMPD_LIST_H

typedef struct
{
  int    size;       // Count of allocated slots
  int    length;     // Count of used slots
  void **items;
} list;

list *list_new(int size);
int   list_search(const list *l, void *ptr);
void  list_remove(list *l, int i);
void  list_push(list *l, void *item);
void *list_pop(list *l);
void *list_shift(list *l);
void  list_free(list *l);

static inline int list_get_length(const list *l)
{
  return l->length;
}

static inline void *list_get_item(const list *l, int i)
{
  return (i < l->length) ? l->items[i] : NULL;
}

#endif
