#include "ministompd.h"
#include "tomlvalue.h"
#include "list.h"
#include "hash.h"

#include <stdio.h>
#include <inttypes.h>  // PRId64
#include <string.h>
#include <assert.h>

void tomlvalue_free(tomlvalue *v)
{
  switch (v->type)
  {
  case TOML_TYPE_NONE:
  case TOML_TYPE_INT:
  case TOML_TYPE_FLOAT:
  case TOML_TYPE_BOOL:
    xfree(v);
    break;
  case TOML_TYPE_STR:
   {
     bytestring_free(v->u.strval);
     xfree(v);
   }
   break;
  case TOML_TYPE_ARRAY:
    {
      tomlvalue *e;
      while ((e = list_pop(v->u.arrayval)))
        tomlvalue_free(e);
      list_free(v->u.arrayval);
      xfree(v);
    }
    break;
  case TOML_TYPE_TABLE:
    {
      tomlvalue *e;
      while ((e = hash_get_any(v->u.tableval, NULL)))
        tomlvalue_free(e);
      hash_free(v->u.tableval);
      xfree(v);
    }
    break;
  default:
    abort();
  }
}

void tomlvalue_dump(tomlvalue *v, int indent)
{
  for (int i = 0; i < indent; i++) printf("  ");

  switch (v->type)
  {
  case TOML_TYPE_NONE:
    printf("(none)");
    break;
  case TOML_TYPE_STR:
    {
      printf("STR length %zu value '", bytestring_get_length(v->u.strval));
      fwrite(bytestring_get_bytes(v->u.strval), bytestring_get_length(v->u.strval), 1, stdout);
      printf("'\n");
    }
    break;
  case TOML_TYPE_INT:
    printf("INT %" PRId64 "\n", v->u.intval);
    break;
  case TOML_TYPE_FLOAT:
    printf("FLOAT %f %e %a\n", v->u.floatval, v->u.floatval, v->u.floatval);
    break;
  case TOML_TYPE_BOOL:
    printf("BOOL %s\n", (v->u.boolval) ? "true" : "false");
    break;
  case TOML_TYPE_ARRAY:
    {
      int len = list_get_length(v->u.arrayval);
      printf("ARRAY length %d\n", len);
      for (int i = 0; i < len; i++)
      {
        tomlvalue *kid = list_get_item(v->u.arrayval, i);
        tomlvalue_dump(kid, indent + 1);
      }
    }
    break;
  case TOML_TYPE_TABLE:
    {
      int count = hash_get_itemcount(v->u.tableval);

      const bytestring *keys[count];
      hash_get_keys(v->u.tableval, keys, count);

      printf("TABLE item count %d\n", count);
      for (int i = 0; i < count; i++)
      {
        for (int j = 0; j < indent; j++) printf("  ");
        printf("item %d key '", i);
        fwrite(bytestring_get_bytes(keys[i]), bytestring_get_length(keys[i]), 1, stdout);
        printf("':\n");

        tomlvalue *kid = hash_get(v->u.tableval, keys[i]);
        tomlvalue_dump(kid, indent + 1);
      }
    }
    break;
  default:
    printf("UNKNOWN TYPE %d\n", v->type);
  }
}

tomlvalue *tomlvalue_new(void)
{
  tomlvalue *v = xmalloc(sizeof(struct tomlvalue));
  v->type = TOML_TYPE_NONE;
  return v;
}

tomlvalue *tomlvalue_new_table(void)
{
  struct tomlvalue *v = xmalloc(sizeof(struct tomlvalue));
  v->type = TOML_TYPE_TABLE;
  v->u.tableval = hash_new(8);
  return v;
}

tomlvalue *tomlvalue_new_array(void)
{
  struct tomlvalue *v = xmalloc(sizeof(struct tomlvalue));
  v->type = TOML_TYPE_ARRAY;
  v->u.arrayval = list_new(8);
  return v;
}

