#include <string.h>  // strlen()
#include "ministompd.h"

headerbundle *headerbundle_new(void)
{
  headerbundle *hb = xmalloc(sizeof(headerbundle));

  hb->count   = 0;
  hb->size    = 16;  // A reasonable number of headers for most frames
  hb->headers = xmalloc(sizeof(struct header) * hb->size);

  return hb;
}

// Resize to at least the given size
static void headerbundle_resize(headerbundle *hb, int size)
{
  // Don't bother if already the given size or larger
  if (size <= hb->size)
    return;

  while (size < hb->size)
    hb->size *= 2;

  hb->headers = xrealloc(hb->headers, sizeof(struct header) * hb->size);
}

// Prepends a key/value pair to the start of the bundle. We take ownership
//  of the bytestring arguments.
void headerbundle_prepend_header(headerbundle *hb, bytestring *key, bytestring *val)
{
  // Ensure we have room
  headerbundle_resize(hb, hb->count + 1);

  // Move each header down by one slot
  for (int i = hb->count; i > 0; i--)
  {
    hb->headers[i].key = hb->headers[i - 1].key;
    hb->headers[i].val = hb->headers[i - 1].val;
  }

  // Add new entry at the beginning
  hb->headers[0].key = key;
  hb->headers[0].val = val;
  hb->count++;
}

// Appends a key/value pair to the end of the bundle. We take ownership of
//  the bytestring arguments.
void headerbundle_append_header(headerbundle *hb, bytestring *key, bytestring *val)
{
  // Ensure we have room
  headerbundle_resize(hb, hb->count + 1);

  // Add new entry at the end
  hb->headers[hb->count].key = key;
  hb->headers[hb->count].val = val;
  hb->count++;
}

// Returns true iff the bundle has a header with the given index. If the key
//  and/or val pointers are provided, they will be filled in with a pointer
//  to the respective bytestring.
bool headerbundle_get_header(headerbundle *hb, int index, const bytestring **key, const bytestring **val)
{
  // Bounds check
  if ((index < 0) || (index >= hb->count))
    return false;  // No such header

  // Fill in pointers, if needed
  if (key)
    *key = hb->headers[index].key;
  if (val)
    *val = hb->headers[index].val;

  return true;
}

// Returns the value of the first header in the bundle whose key matches the
//  given string. Returns NULL on no match.
const bytestring *headerbundle_get_header_value_by_str(headerbundle *hb, const char *key)
{
  int keylen = strlen(key);

  for (int i = 0; i < hb->count; i++)
  {
    bytestring *headerkey = hb->headers[i].key;
    if (bytestring_get_length(headerkey) == keylen)
    {
      if (bytestring_equals_bytes(headerkey, (uint8_t *) key, keylen))
        return hb->headers[i].val;
    }
  }

  return NULL;  // No match
}

void headerbundle_dump(headerbundle *hb)
{
  printf("headerbundle at %p count %d size %d\n", hb, hb->count, hb->size);

  for (int i = 0; i < hb->count; i++)
  {
    printf("Header %d:\n", i);
    bytestring_dump(hb->headers[i].key);
    bytestring_dump(hb->headers[i].val);
  }
}

void headerbundle_free(headerbundle *hb)
{
  // Free header contents
  for (int i = 0; i < hb->count; i++)
  {
    bytestring_free(hb->headers[i].key);
    bytestring_free(hb->headers[i].val);
  }

  // Free array of header structs
  xfree(hb->headers);

  // Free root struct
  xfree(hb);
}
