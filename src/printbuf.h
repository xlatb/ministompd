
struct printbuf
{
  char  *buf;
  size_t size;
  size_t length;
};

typedef struct printbuf printbuf;

printbuf *printbuf_new(int size);
printbuf *printbuf_append_chars(printbuf *pb, const char *src, size_t len);
int       printbuf_truncate(printbuf *pb, int len);
printbuf *printbuf_append_int(printbuf *pb, int i);
printbuf *printbuf_printf(printbuf *pb, char *fmt, ...);
printbuf *printbuf_vprintf(printbuf *pb, char *fmt, va_list args);
