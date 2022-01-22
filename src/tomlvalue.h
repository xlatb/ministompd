#include "list.h"
#include "hash.h"
#include "buffer.h"

#include <stdio.h>

#ifndef MINISTOMPD_TOMLVALUE_H
#define MINISTOMPD_TOMLVALUE_H

struct tomlvalue_packed_datetime
{
  unsigned int tzsign    : 1;   // Separate sign and magnitude so that
  unsigned int tzminutes : 11;  //  -00:00 is distinct from +00:00.
  int64_t      msecs     : 52;  // Milliseconds since start of epoch year.
};

struct tomlvalue_unpacked_datetime
{
  bool tzneg;
  int  tzminutes;

  int  year;
  int  month;
  int  day;
  int  hour;
  int  minute;
  int  second;
  int  msec;
};

// Times are stored as signed milliseconds
#define TOML_TIME_SECOND INT64_C(1000)
#define TOML_TIME_MINUTE (TOML_TIME_SECOND * 60)
#define TOML_TIME_HOUR   (TOML_TIME_MINUTE * 60)
#define TOML_TIME_DAY    (TOML_TIME_HOUR * 24)

// Number of digits in the fractional part of a time
// NOTE: This should equal log10(TOML_TIME_SECOND)
#define TOML_TIME_FRAC_DIGITS 3

// Times are relative to the beginning of this year
#define TOML_TIME_EPOCH_YEAR 1970

enum tomlvalue_type
{
  TOML_TYPE_NONE        = 0x00,

  // Simple
  TOML_TYPE_STR         = 0x01,
  TOML_TYPE_INT         = 0x02,
  TOML_TYPE_FLOAT       = 0x03,
  TOML_TYPE_BOOL        = 0x04,

  // Date/time
  TOML_TYPE_DATETIME_TZ = 0x11,
  TOML_TYPE_DATETIME    = 0x12,
  TOML_TYPE_DATE        = 0x13,
  TOML_TYPE_TIME        = 0x14,

  // Container
  TOML_TYPE_ARRAY       = 0x21,
  TOML_TYPE_TABLE       = 0x22
};

#define TOML_TYPESET_MASK      0xF0
#define TOML_TYPESET_SIMPLE    0x00
#define TOML_TYPESET_DATETIME  0x10
#define TOML_TYPESET_CONTAINER 0x20

#define TOML_FLAG_AUTOVIVIFIED 0x01
#define TOML_FLAG_CLOSED       0x02

struct tomlvalue
{
//  enum tomlvalue_type type;
  union
  {
    bytestring *strval;
    int64_t     intval;
    double      floatval;
    bool        boolval;
    list       *arrayval;
    hash       *tableval;
    struct tomlvalue_packed_datetime datetimeval;
  } u;
  uint8_t type;
  uint8_t flags;
};

typedef struct tomlvalue tomlvalue;

void tomlvalue_free(tomlvalue *v);
void tomlvalue_dump(tomlvalue *v, int indent);

tomlvalue *tomlvalue_new(void);
tomlvalue *tomlvalue_new_table(void);
tomlvalue *tomlvalue_new_array(void);

const char *tomlvalue_unpacked_datetime_check(struct tomlvalue_unpacked_datetime *unpacked);

bool tomlvalue_get_datetime(tomlvalue *v, struct tomlvalue_unpacked_datetime *unpacked);
bool tomlvalue_set_datetime(tomlvalue *v, struct tomlvalue_unpacked_datetime *unpacked);

bool tomlvalue_get_tzoffset(tomlvalue *v, bool *negative, int *minutes);

static inline tomlvalue *tomlvalue_get_hash_element(tomlvalue *v, bytestring *key)
{
  if (v->type != TOML_TYPE_TABLE)
    return NULL;

  return hash_get(v->u.tableval, key);
}

static inline int tomlvalue_get_array_length(tomlvalue *v)
{
  if (v->type != TOML_TYPE_ARRAY)
    return 0;

  return list_get_length(v->u.arrayval);
}

static inline tomlvalue *tomlvalue_get_array_element(tomlvalue *v, int index)
{
  if (v->type != TOML_TYPE_ARRAY)
    return NULL;

  int len = list_get_length(v->u.arrayval);
  if ((index < 0) || (index >= len))
    return NULL;

  return list_get_item(v->u.arrayval, index);
}

void tomlvalue_set_flag(tomlvalue *v, int flag, bool value);
bool tomlvalue_get_flag(tomlvalue *v, int flag);

#endif
