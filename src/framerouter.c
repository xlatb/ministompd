#include "ministompd.h"
#include <string.h>  // memmove()

framerouter *framerouter_new()
{
  framerouter *fr = xmalloc(sizeof(framerouter));

  fr->size     = FRAMEROUTER_DEFAULT_SIZE;
  fr->length   = 0;
  fr->position = 0;

  fr->subscriptions = xmalloc(sizeof(subscription *) * fr->size);

  return fr;
}

void framerouter_free(framerouter *fr)
{
  // Note: We do not free the individual subscriptions

  xfree(fr->subscriptions);

  xfree(fr);
}

static void framerouter_resize(framerouter *fr, int size)
{
  size = (size | 0x07) + 1;  // Round up to next multiple of eight

  if (fr->size >= size)
    return;  // Nothing to do

  fr->size = size;
  fr->subscriptions = xrealloc(fr->subscriptions, sizeof(subscription *) * fr->size);
}

// Adds a subscription to the framerouter. Does not take ownership of the
//  subscription.
void framerouter_add_subscription(framerouter *fr, subscription *sub)
{
  if (fr->length == fr->size)
    framerouter_resize(fr, fr->size + 1);

  fr->subscriptions[fr->length] = sub;

  fr->length++;
}

// Removes a subscription from the framerouter. Returns true iff subscription
//  was removed.
bool framerouter_remove_subscription(framerouter *fr, subscription *sub)
{
  int i;
  bool found = false;

  // Search for subscription
  for (i = 0; i < fr->length; i++)
  {
    if (fr->subscriptions[i] == sub)
    {
      found = true;
      break;
    }
  }

  // Give up if not found
  if (found == false)
    return false;

  // Shift entries down to fill in vacated slot
  memmove(fr->subscriptions + i, fr->subscriptions + i + 1, sizeof(subscription *) * (fr->length - i - 1));
  fr->length--;

  return true;
}
