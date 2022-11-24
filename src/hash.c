#define _XOPEN_SOURCE 500  // random()

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>     // random()
#include <string.h>     // memset()
#include <time.h>       // time()
#include <sys/types.h>  // open()
#include <sys/stat.h>   // open()
#include <fcntl.h>      // open()
#include <assert.h>     // assert()
#include "siphash24.h"
#include "ministompd.h"

static bool    siphash_key_set;
static uint8_t siphash_key[16];

static bool set_siphash_key_devurandom(void)
{
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1)
    return false;

  bool success = (read(fd, siphash_key, sizeof(siphash_key)) == sizeof(siphash_key));

  close(fd);

  return success;
}

static bool set_siphash_key_posix(void)
{
  srandom((unsigned int) time(NULL));

  for (int i = 0; i < sizeof(siphash_key); i++)
    siphash_key[i] = random() & 0xFF;

  return true;
}

static void ensure_siphash_key(void)
{
  if (siphash_key_set)
    return;  // Already have a key

  if (!set_siphash_key_devurandom())
    set_siphash_key_posix();

  siphash_key_set = true;
}

// Rounds an integer up to the next greater-or-equal power of two.
static uint32_t round_up_pow2(uint32_t v)
{
  int pos;

  // Special cases
  if (v == 0)
    return 1;
  else if ((v & (v - 1)) == 0)
    return v;  // Already a power of two

  // Find the highest byte with at least one bit set
  if (v & 0xFFFF0000UL)
    pos = (v & 0xFF000000UL) ? 3 : 2;
  else
    pos = (v & 0x0000FF00UL) ? 1 : 0;

  // Isolate the byte in question
  unsigned int byte = v >> (pos << 3);

  // Copy the highest bit into all lower bits
  byte |= byte >> 1;
  byte |= byte >> 2;
  byte |= byte >> 4;

  // Increment to next power of two and shift back into place
  v = (byte + 1) << (pos << 3);

  return v;
}

hash *hash_new(int sizehint)
{
  // Generate siphash key, if we don't have one yet
  if (!siphash_key_set)
    ensure_siphash_key();

  // Find size (SipHash may work best when the table is a power of two in size)
  int bucketcount = round_up_pow2(sizehint);
  if (bucketcount < HASH_MIN_BUCKETS)
    bucketcount = HASH_MIN_BUCKETS;

  hash *h = xmalloc(sizeof(hash));
  h->bucketcount = bucketcount;
  h->buckets     = xmalloc(sizeof(hash_item) * bucketcount);
  h->itemcount   = 0;

  memset(h->buckets, 0, (sizeof(hash_item) * bucketcount));

  return h;
}

void hash_free(hash *h)
{
  // Hash must be empty before calling
  assert(h->itemcount == 0);

  xfree(h->buckets);
  xfree(h);
}

void hash_grow(hash *h)
{
  // Don't grow if already at max size
  if (h->bucketcount >= HASH_MAX_BUCKETS)
    return;

  // Calculate new size
  int bucketcount = h->bucketcount << 1;
  if ((bucketcount < 0) || (bucketcount >= HASH_MAX_BUCKETS))
    bucketcount = HASH_MAX_BUCKETS;

  fprintf(stderr, "Expanding hash %d -> %d\n", h->bucketcount, bucketcount);

  // Allocate new buckets
  hash_item *buckets = xmalloc(sizeof(hash_item) * bucketcount);
  memset(buckets, 0, (sizeof(hash_item) * bucketcount));

  // Copy from old to new buckets
  for (int b = 0; b < h->bucketcount; b++)
  {
    for (hash_item *olditem = &h->buckets[b]; olditem && olditem->key; olditem = olditem->next)
    {
      // Re-hash existing key
      uint64_t code;
      siphash_24_crypto_auth((unsigned char *) &code, bytestring_get_bytes(olditem->key), bytestring_get_length(olditem->key), siphash_key);

      // Find the associated bucket for the new size
      int b = code % bucketcount;

      // Add to bucket
      hash_item *item = &buckets[b];
      if (item->key)
      {
        // Find end of chain for the bucket
        while (item->next)
          item = item->next;

        // Add new item
        hash_item *new_item = xmalloc(sizeof(hash_item));
        new_item->key  = olditem->key;
        new_item->val  = olditem->val;
        new_item->next = NULL;
        item->next = new_item;
      }
      else
      {
        // No chain, so just populate head
        item->key = olditem->key;
        item->val = olditem->val;
        item->next = NULL;
      }
    }
  }

  // Deallocate old allocated items
  for (int b = 0; b < h->bucketcount; b++)
  {
    hash_item *bucket = &h->buckets[b];

    hash_item *nextitem;
    for (hash_item *olditem = bucket->next; olditem; olditem = nextitem)
    {
      nextitem = olditem->next;

      xfree(olditem);
    }
  }

  // Replace bucket data
  xfree(h->buckets);
  h->buckets = buckets;
  h->bucketcount = bucketcount;

  return;
}

// Adds a value to the hash. We do not take ownership of the key. The value
//  must remain valid as long as it is in the hash. Returns false if the
//  addition failed due to already having a value with the given key.
bool hash_add(hash *h, const bytestring *key, void *value)
{
  // Grow the hash if too full
  float load = h->itemcount / h->bucketcount;
  if (load > HASH_LOAD_FACTOR)
    hash_grow(h);

  // Find the hash value
  uint64_t code;
  siphash_24_crypto_auth((unsigned char *) &code, bytestring_get_bytes(key), bytestring_get_length(key), siphash_key);

  // Find the associated bucket
  int b = code % h->bucketcount;

  //printf("Hash code %" PRIx64 " bucket %d\n", code, b);

  // Add item to list
  hash_item *item = &h->buckets[b];
  if (item->key)
  {
    // At least one item in this bucket, so walk the list
    while (item)
    {
      if (bytestring_equals(item->key, key))
        return false;  // Key already exists in hash
      else if (item->next)
        item = item->next;
      else
      {
        hash_item *new_item = xmalloc(sizeof(hash_item));
        new_item->key  = bytestring_dup(key);
        new_item->val  = value;
        new_item->next = NULL;

        item->next = new_item;
        h->itemcount++;
        return true;
      }
    }
  }
  else
  {
    // Nothing in this bucket, so new item is the list head
    item->key  = bytestring_dup(key);
    item->val  = value;
    item->next = NULL;
  }

  h->itemcount++;
  return true;
}

void *hash_replace(hash *h, const bytestring *key, void *value)
{
  // TODO: This could be more efficient
  void *oldvalue = hash_remove(h, key);
  hash_add(h, key, value);
  return oldvalue;
}

// Searches the hash for the key. If it is found, returns the associated
//  value. If it is not found, returns NULL.
void *hash_get(hash *h, const bytestring *key)
{
  // Find the hash value
  uint64_t code;
  siphash_24_crypto_auth((unsigned char *) &code, bytestring_get_bytes(key), bytestring_get_length(key), siphash_key);

  // Find the associated bucket
  int b = code % h->bucketcount;

  // Walk the list looking for the item
  hash_item *item = &h->buckets[b];
  while (item && item->key)
  {
    if (bytestring_equals(item->key, key))
      return item->val;

    item = item->next;
  }

  // Not found
  return NULL;
}

// As above but the key is raw pointer to bytes plus a length.
void *hash_get_raw(hash *h, const uint8_t *keybytes, size_t keylength)
{
  // Find the hash value
  uint64_t code;
  siphash_24_crypto_auth((unsigned char *) &code, keybytes, keylength, siphash_key);

  // Find the associated bucket
  int b = code % h->bucketcount;

  // Walk the list looking for the item
  hash_item *item = &h->buckets[b];
  while (item && item->key)
  {
    if (bytestring_equals_bytes(item->key, keybytes, keylength))
      return item->val;

    item = item->next;
  }

  // Not found
  return NULL;
}

// Retrieves an item from the hash, returning its value, or NULL if the
//  hash was empty.
// If keyptr is supplied, a pointer to the item's key will be stored there.
void *hash_get_any(hash *h, const bytestring **keyptr)
{
  if (h->itemcount == 0)
    return NULL;

  // Find any non-empty bucket
  for (int b = 0; b < h->bucketcount; b++)
  {
    hash_item *item = &h->buckets[b];

    // Skip empty buckets
    if (!item->key)
      continue;

    // Found one, remember its contents
    hash_item retrieved = *item;

    // If the caller provided a keyptr, give them the key
    if (keyptr)
      *keyptr = (bytestring *) retrieved.key;

    return retrieved.val;
  }

  // Should never happen, since itemcount was nonzero
  abort();
}

// Removes the given key from the hash, returning the value it was previously
//  associated with, if any.
void *hash_remove(hash *h, const bytestring *key)
{
  // Find the hash value
  uint64_t code;
  siphash_24_crypto_auth((unsigned char *) &code, bytestring_get_bytes(key), bytestring_get_length(key), siphash_key);

  // Find the associated bucket
  int b = code % h->bucketcount;

  // Remove item from list
  hash_item *prev = NULL;
  hash_item *item = &h->buckets[b];
  if (item->key)
  {
    // At least one item in this bucket, so walk the list
    while (item)
    {
      if (bytestring_equals(item->key, key))
      {
        // Found, remove from list
        bytestring_free((bytestring *)item->key);
        void *val = item->val;

        if (prev)
        {
          // Remove from list interior or end
          prev->next = item->next;
        }
        else if (item->next)
        {
          // Replace list head
          hash_item *dead = item->next;
          *item = *item->next;
          xfree(dead);
        }
        else
        {
          // Clear single-element list
          item->key  = NULL;
          item->val  = NULL;
        }

        h->itemcount--;
        return val;
      }
      else if (item->next)
      {
        prev = item;
        item = item->next;
      }
      else
      {
        // Not found in list
        return NULL;
      }
    }
  }
  else
  {
    // Nothing in the bucket, so key not found
    return NULL;
  }

  return NULL;
}

// Removes any given item from the hash, returning its value, or NULL if the
//  hash was empty.
// If keyptr is supplied, a pointer to the item's key will be stored there.
//  The caller then gains ownership of this.
// If keyptr is not supplied, the key is freed.
void *hash_remove_any(hash *h, bytestring **keyptr)
{
  if (h->itemcount == 0)
    return NULL;

  // Find any non-empty bucket
  for (int b = 0; b < h->bucketcount; b++)
  {
    hash_item *item = &h->buckets[b];

    // Skip empty buckets
    if (!item->key)
      continue;

    // Found one, remember its contents
    hash_item removed = *item;

    // Delete it from the list
    if (item->next)
    {
      // Replace list head
      *item = *item->next;
      xfree(removed.next);
    }
    else
    {
      // Clear single-element list
      item->key  = NULL;
      item->val  = NULL;
    }

    // If the caller provided a keyptr, give them the key
    if (keyptr)
      *keyptr = (bytestring *) removed.key;
    else
      bytestring_free((bytestring *) removed.key);

    // Record-keeping
    h->itemcount--;

    return removed.val;
  }

  // Should never happen, since itemcount was nonzero
  abort();
}

int hash_get_itemcount(hash *h)
{
  return h->itemcount;
}

// Given a hash, an array of 'const bytestring *' and a max size, writes the
//  hash keys to the array. The array size must be >= 1.
// Returns the number of entries written.
// The original keys are returned, and the hash keeps ownership of them.
// Unlike hash_get_keys_list(), we can use this to enumerate the keys
//  without allocating new memory on the heap.
int hash_get_keys(hash *h, const bytestring **list, int size)
{
  int count = 0;

  for (int b = 0; b < h->bucketcount; b++)
  {
    hash_item *item = &h->buckets[b];

    while (item && item->key)
    {
      *list++ = item->key;
      item = item->next;
      count++;

      // Enforce maximum size
      if (count == size)
        return count;
    }
  }

  return count;
}

void hash_dump(hash *h)
{
  printf("== hash @ %p buckets %d items %d\n", h, h->bucketcount, h->itemcount);

  for (int b = 0; b < h->bucketcount; b++)
  {
    printf("bucket %d\n", b);

    hash_item *item = &h->buckets[b];
    while (item && item->key)
    {
      printf("  ");
      bytestring_dump(item->key);

      item = item->next;
    }
  }
}
