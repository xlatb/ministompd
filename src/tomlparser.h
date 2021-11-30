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
struct toml_error *tomlparser_pull_error(tomlparser *tp);
void               tomlparser_set_file(tomlparser *tp, FILE *f);
bool               tomlparser_parse_statement(tomlparser *tp);

#endif
