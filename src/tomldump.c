#include "tomlparser.h"
#include "alloc.h"

#include <string.h>
#include <stdio.h>
#include <math.h>  // isnan()
#include <inttypes.h>  // PRId64

void sushi_dump_value(tomlvalue *v);

void sushi_dump_bytestring(const bytestring *bs)
{
  size_t len = bytestring_get_length(bs);

  putchar('"');

  for (int i = 0; i < len; i++)
  {
    uint8_t b = bytestring_get_byte(bs, i);
    if (b == '\\')
      printf("\\\\");
    else if (b == '"')
      printf("\\\"");
    else if (b < 32)
      printf("\\u%04X", b);
    else
      putchar(b);
  }

  putchar('"');
}

void sushi_dump_value_table(tomlvalue *v)
{
  int count = hash_get_itemcount(v->u.tableval);

  const bytestring *keys[count];
  hash_get_keys(v->u.tableval, keys, count);

  printf("{\n");

  for (int i = 0; i < count; i++)
  {
    sushi_dump_bytestring(keys[i]);
    printf(": ");

    tomlvalue *kid = hash_get(v->u.tableval, keys[i]);
    sushi_dump_value(kid);

    if (i < count - 1)
      printf(",");

    printf("\n");
  }

  printf("}\n");
}

void sushi_dump_value_array(tomlvalue *v)
{
  int count = list_get_length(v->u.arrayval);

  printf("[\n");

  for (int i = 0; i < count; i++)
  {
    tomlvalue *kid = list_get_item(v->u.arrayval, i);
    sushi_dump_value(kid);

    if (i < count - 1)
      printf(",");

    printf("\n");
  }

  printf("]\n");
}

void sushi_dump_value(tomlvalue *v)
{
  switch (v->type)
  {
  case TOML_TYPE_INT:
    printf("{\"type\":\"integer\",\"value\":\"%" PRId64 "\"}", v->u.intval);
    break;
  case TOML_TYPE_FLOAT:
    if (isnan(v->u.floatval))
      printf("{\"type\":\"float\",\"value\":\"nan\"}");  // toml-test strips the sign from the nan
    else
      printf("{\"type\":\"float\",\"value\":\"%.16g\"}", v->u.floatval);
    break;
  case TOML_TYPE_BOOL:
    printf("{\"type\":\"bool\",\"value\":");
    if (v->u.boolval)
      printf("\"true\"");
    else
      printf("\"false\"");
    printf("}");
    break;
  case TOML_TYPE_STR:
    printf("{\"type\":\"string\",\"value\":");
    sushi_dump_bytestring(v->u.strval);
    printf("}");
    break;
  case TOML_TYPE_DATE:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("{\"type\":\"date-local\",\"value\":\"%04d-%02d-%02d\"}", unpacked.year, unpacked.month, unpacked.day);
    }
    break;
  case TOML_TYPE_TIME:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("{\"type\":\"time-local\",\"value\":\"%02d:%02d:%02d.%03d\"}", unpacked.hour, unpacked.minute, unpacked.second, unpacked.msec);
    }
    break;
  case TOML_TYPE_DATETIME:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("{\"type\":\"datetime-local\",\"value\":\"%04d-%02d-%02dT%02d:%02d:%02d\"}", unpacked.year, unpacked.month, unpacked.day, unpacked.hour, unpacked.minute, unpacked.second);
    }
    break;
  case TOML_TYPE_DATETIME_TZ:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("{\"type\":\"datetime\",\"value\":\"%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d\"}", unpacked.year, unpacked.month, unpacked.day, unpacked.hour, unpacked.minute, unpacked.second, unpacked.tzneg ? '-' : '+', unpacked.tzminutes / 60, unpacked.tzminutes % 60);
    }
    break;
  case TOML_TYPE_TABLE:
    sushi_dump_value_table(v);
    break;
  case TOML_TYPE_ARRAY:
    sushi_dump_value_array(v);
    break;
  default:
    fprintf(stderr, "Unknown type %d\n", v->type);
    abort();
  }
}

void sushi_process(FILE *f)
{
  tomlparser *tp = tomlparser_new();
  tomlparser_set_file(tp, f);

  while (tomlparser_parse_statement(tp))
    ;

  int errorcount = tomlparser_get_error_count(tp);
  if (errorcount)
  {
    printf("%d errors:\n", errorcount);

    struct toml_error *err;
    while ((err = tomlparser_pull_error(tp)))
    {
      printf("Line %d: %s\n", err->line, err->message);
      xfree(err);
    }
    exit(1);
  }

  sushi_dump_value(tomlparser_pull_data(tp));

  tomlparser_free(tp);

  exit(0);
}

int main(int argc, char *argv[])
{
  if (argc <= 1)
  {
    printf("Usage:  %s [ file ...]\n", argv[0]);
    printf("        %s --sushi\n", argv[0]);
    printf("  --sushi For use with BurntSushi TOML tester (https://github.com/BurntSushi/toml-test)\n");
    return 1;
  }

  bool sushi = false;

  // Handle command-line switches
  int argindex = 1;
  while (argindex < argc)
  {
    if (!strcmp(argv[argindex], "--sushi"))
      sushi = true;
    else
      break;

    argindex++;
  }

  if (sushi)
  {
    if (argindex < argc)
    {
      fprintf(stderr, "Sushi mode only reads from stdin. Do not supply extra arguments.\n");
      return 1;
    }

    sushi_process(stdin);
    abort();  // Not reached
  }

  for (int i = 1; i < argc; i++)
  {
    printf("--- File: %s\n", argv[i]);

    FILE *f = fopen(argv[i], "r");
    if (!f)
    {
      perror("open()");
      return 1;
    }

    tomlparser *tp = tomlparser_new();
    tomlparser_set_file(tp, f);

    if (tomlparser_parse(tp))
    {
      printf("\nOutput:\n");
      tomlvalue *v = tomlparser_pull_data(tp);
      tomlvalue_dump(v, 0);
      tomlvalue_free(v);
    }
    else
    {
      struct toml_error *err;
      printf("\nErrors:\n");
      while ((err = tomlparser_pull_error(tp)))
      {
        printf("Line %d: %s\n", err->line, err->message);
        xfree(err);
      }
    }

    tomlparser_free(tp);
  }
}
