#include <string.h>  // memmove()

#include "ministompd.h"

bytestring_list *bytestring_list_new(size_t size)
{
  // Size must be positive
  if (size < 1)
    size = 1;

  bytestring_list *list = xmalloc(sizeof(bytestring_list));
  const bytestring **strings = xmalloc(size * sizeof(bytestring *));

  list->size    = size;
  list->length  = 0;
  list->strings = strings;

  return list;
}

void bytestring_list_free(bytestring_list *list)
{
  for (int i = 0; i < list->length; i++)
    bytestring_free((bytestring *) list->strings[i]);

  xfree(list->strings);

  xfree(list);
}

void bytestring_list_ensure_size(bytestring_list *list, size_t size)
{
  if (size <= list->size)
    return;

  // List is too small, grow it
  while (list->size < size)
    list->size <<= 1;
  list->strings = xrealloc(list->strings, list->size * sizeof(bytestring *));

  return;
}

// Takes ownership of the given bytestring, and adds it to the end of the list.
void bytestring_list_push(bytestring_list *list, const bytestring *bs)
{
  list->length++;
  bytestring_list_ensure_size(list, list->length);

  list->strings[list->length - 1] = bs;
}

// Takes ownership of the given bytestring, and adds it to the beginning of the list.
void bytestring_list_unshift(bytestring_list *list, const bytestring *bs)
{
  list->length++;
  bytestring_list_ensure_size(list, list->length);

  memmove(list->strings + 1, list->strings, sizeof(bytestring *) * list->length - 1);

  list->strings[0] = bs;
}

// Returns a pointer to the bytestring at the given position in the list. The
//  list keeps ownership of the string.
const bytestring *bytestring_list_get(bytestring_list *list, int index)
{
  // Bounds check
  if ((index < 0) || (index >= list->length))
    return NULL;

  return list->strings[index];
}
