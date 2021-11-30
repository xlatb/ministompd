#include "tomlparser.h"
#include "alloc.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
  if (argc <= 1)
  {
    printf("Usage: %s <toml-file> ...\n", argv[0]);
    return 1;
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

    while (tomlparser_parse_statement(tp))
      ;

    struct toml_error *err;
    printf("Errors:\n");
    while ((err = tomlparser_pull_error(tp)))
    {
      printf("Line %d: %s\n", err->line, err->message);
      xfree(err);
    }

    printf("Output:\n");
    tomlvalue_dump(tp->root, 0);
  }
}
