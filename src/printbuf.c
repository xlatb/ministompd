#include <stdlib.h>
#include <assert.h>
#include "bytestring.h"
#include "printbuf.h"
#include "alloc.h"

#define PRINTBUF_COUNT 6

struct printbuf
{
  bool        acquired;
  bytestring *bs;
};

static struct printbuf *printbufs = NULL;

static void printbuf_setup(void)
{
  printbufs = xmalloc(sizeof(struct printbuf) * PRINTBUF_COUNT);

  for (int i = 0; i < PRINTBUF_COUNT; i++)
  {
    printbufs[i].acquired = false;
    printbufs[i].bs       = bytestring_new(2048);
  }
}

bytestring *printbuf_acquire(void)
{
  if (printbufs == NULL)
    printbuf_setup();

  for (int i = 0; i < PRINTBUF_COUNT; i++)
  {
    if (printbufs[i].acquired == false)
    {
      printbufs[i].acquired = true;
      bytestring_truncate(printbufs[i].bs, 0);
      return printbufs[i].bs;
    }
  }

  abort();  // Out of printbufs
}

void printbuf_release(bytestring *bs)
{
  if (printbufs == NULL)
    abort();

  for (int i = 0; i < PRINTBUF_COUNT; i++)
  {
    if (printbufs[i].bs == bs)
    {
      assert(printbufs[i].acquired == true);

      printbufs[i].acquired = false;
      return;
    }
  }

  abort();  // Not found
}
