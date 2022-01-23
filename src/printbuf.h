#ifndef MINISTOMPD_PRINTBUF_H
#define MINISTOMPD_PRINTBUF_H

bytestring *printbuf_acquire(void);
void        printbuf_release(bytestring *bs);

#endif
