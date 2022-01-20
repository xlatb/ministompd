#include "ministompd.h"
#include <string.h>  // memmove()

#define FRAMEROUTER_DEFAULT_SUBS_SIZE 8
#define FRAMEROUTER_DEFAULT_DISP_SIZE 4

static subscription *framerouter_find_subscription(framerouter *fr)
{
  int sub_count = list_get_length(fr->subscriptions);
  if (sub_count == 0)
    return NULL;  // No subscriptions

  fr->subscription_index %= sub_count;

  subscription *sub = list_get_item(fr->subscriptions, fr->subscription_index);

  fr->subscription_index++;

  return sub;
}

framerouter *framerouter_new()
{
  framerouter *fr = xmalloc(sizeof(framerouter));

  fr->subscriptions      = list_new(FRAMEROUTER_DEFAULT_SUBS_SIZE);
  fr->subscription_index = 0;

  fr->dispatches = list_new(FRAMEROUTER_DEFAULT_DISP_SIZE);

  return fr;
}

void framerouter_free(framerouter *fr)
{
  // Note: We do not free the individual subscriptions because we do not own them
  list_free(fr->subscriptions);

  // TODO: Free the contained dispatches
  list_free(fr->dispatches);

  xfree(fr);
}

// Adds a subscription to the framerouter. Does not take ownership of the
//  subscription.
void framerouter_add_subscription(framerouter *fr, subscription *sub)
{
  list_push(fr->subscriptions, sub);
}

// Removes a subscription from the framerouter. Returns true iff subscription
//  was removed.
bool framerouter_remove_subscription(framerouter *fr, subscription *sub)
{
  int i = list_search(fr->subscriptions, sub);
  if (i < 0)
    return false;  // Not found

  list_remove(fr->subscriptions, i);
  return true;
}

int framerouter_subscription_count(framerouter *fr)
{
  return list_get_length(fr->subscriptions);
}

void framerouter_dispatch(framerouter *fr, frame *f, storage_handle sh)
{
  struct dispatch *d = xmalloc(sizeof(struct dispatch));
  d->frame  = f;
  d->handle = sh;
  if (clock_gettime(CLOCK_MONOTONIC, &d->createtime))
    abort();  // Couldn't get time

  list_push(fr->dispatches, d);

  subscription *sub = framerouter_find_subscription(fr);
  assert(sub != NULL);

  subscription_deliver(sub, f);
}
