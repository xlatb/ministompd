#include <stdlib.h>   // size_t

#ifndef MINISTOMPD_BYTESTRING_LIST_H
#define MINISTOMPD_BYTESTRING_LIST_H

typedef struct
{
  size_t             size;      // Allocated bytes
  size_t             length;    // Current number of bytestrings
  const bytestring **strings;   // Array of bytestring pointers
} bytestring_list;

bytestring_list  *bytestring_list_new(size_t size);
void              bytestring_list_free(bytestring_list *list);
void              bytestring_list_push(bytestring_list *list, const bytestring *bs);
void              bytestring_list_unshift(bytestring_list *list, const bytestring *bs);
const bytestring *bytestring_list_get(bytestring_list *list, int index);

static inline size_t bytestring_list_get_length(const bytestring_list *list)
{
  return list->length;
}

#endif
