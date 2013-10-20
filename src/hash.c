#define _XOPEN_SOURCE 500  // random()

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>     // random()
#include <string.h>     // memset()
#include <time.h>       // time()
#include <sys/types.h>  // open()
#include <sys/stat.h>   // open()
#include <fcntl.h>      // open()
#include "siphash24.h"
#include "ministompd.h"

// TODO: Grow hash tables as they fill up
// TODO: Allow enumerating keys

static bool    siphash_key_set;
static uint8_t siphash_key[8];

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
  // TODO
}

// Adds a value to the hash. We do not take ownership of the key. The value
//  must remain valid as long as it is in the hash. Returns false if the
//  addition failed due to already having a value with the given key.
bool hash_add(hash *h, const bytestring *key, void *value)
{
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
          xfree(item->next);
          *item = *item->next;
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

int hash_get_itemcount(hash *h)
{
  return h->itemcount;
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
