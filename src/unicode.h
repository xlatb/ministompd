#include <stdint.h>
#include <stdlib.h>  // size_t

#ifndef MINISTOMPD_UNICODE_H
#define MINISTOMPD_UNICODE_H

size_t unicode_ucs4_to_utf8(uint32_t ucs4, uint8_t (*utf8)[4]);

#endif
