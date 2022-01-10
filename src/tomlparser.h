#include "buffer.h"
#include "tomlvalue.h"

#include <stdio.h>

#ifndef MINISTOMPD_TOMLPARSER_H
#define MINISTOMPD_TOMLPARSER_H

struct toml_error
{
  const char *message;
  int         line;
};

struct tomlparser
{
  FILE   *file;
  int     line;
  buffer *peekbuf;
  list   *errors;

  tomlvalue *root;
  tomlvalue *current;
};

typedef struct tomlparser tomlparser;

tomlparser        *tomlparser_new(void);
void               tomlparser_free(tomlparser *tp);
int                tomlparser_get_error_count(tomlparser *tp);
struct toml_error *tomlparser_pull_error(tomlparser *tp);
tomlvalue         *tomlparser_pull_data(tomlparser *tp);
void               tomlparser_set_file(tomlparser *tp, FILE *f);
bool               tomlparser_parse_statement(tomlparser *tp);
bool               tomlparser_parse(tomlparser *tp);

#endif
