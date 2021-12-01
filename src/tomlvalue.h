#include "list.h"
#include "hash.h"
#include "buffer.h"

#include <stdio.h>

#ifndef MINISTOMPD_TOMLVALUE_H
#define MINISTOMPD_TOMLVALUE_H

enum tomlvalue_type
{
  TOML_TYPE_NONE        = 0x00,

  // Scalars
  TOML_TYPE_STR         = 0x01,
  TOML_TYPE_INT         = 0x02,
  TOML_TYPE_FLOAT       = 0x03,
  TOML_TYPE_BOOL        = 0x04,
  TOML_TYPE_DATETIME_TZ = 0x05,
  TOML_TYPE_DATETIME    = 0x06,
  TOML_TYPE_DATE        = 0x07,
  TOML_TYPE_TIME        = 0x08,

  // Containers
  TOML_TYPE_ARRAY       = 0x11,
  TOML_TYPE_TABLE       = 0x12
};

struct tomlvalue
{
  enum tomlvalue_type type;
  union
  {
    bytestring *strval;
    int64_t     intval;
    double      floatval;
    bool        boolval;
    list       *arrayval;
    hash       *tableval;
  } u;
};

typedef struct tomlvalue tomlvalue;

void tomlvalue_free(tomlvalue *v);
void tomlvalue_dump(tomlvalue *v, int indent);
tomlvalue *tomlvalue_new(void);
tomlvalue *tomlvalue_new_table(void);
tomlvalue *tomlvalue_new_array(void);


#endif
