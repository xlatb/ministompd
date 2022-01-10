#include "bytestring.h"

#ifndef MINISTOMPD_HASH_H
#define MINISTOMPD_HASH_H

#define HASH_MIN_BUCKETS 16
#define HASH_MAX_BUCKETS (1 << 20)
#define HASH_LOAD_FACTOR 0.75

struct hash_item
{
  const bytestring *key;
  void             *val;
  struct hash_item *next;
};
typedef struct hash_item hash_item;

typedef struct
{
  int        bucketcount;
  hash_item *buckets;
  int        itemcount;  // Not directly constrained by the bucketcount
} hash;

hash *hash_new(int sizehint);
void  hash_free(hash *h);
bool  hash_add(hash *h, const bytestring *key, void *value);
void *hash_replace(hash *h, const bytestring *key, void *value);
void *hash_get(hash *h, const bytestring *key);
void *hash_get_raw(hash *h, const uint8_t *key, size_t keylength);
void *hash_get_any(hash *h, const bytestring **keyptr);
void *hash_remove(hash *h, const bytestring *key);
void *hash_remove_any(hash *h, bytestring **keyptr);
int   hash_get_itemcount(hash *h);
int   hash_get_keys(hash *h, const bytestring **list, int size);
void  hash_dump(hash *h);

#endif
