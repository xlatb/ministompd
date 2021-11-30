#include "unicode.h"

size_t unicode_ucs4_to_utf8(uint32_t ucs4, uint8_t (*utf8)[4])
{
  if (ucs4 > 0x10FFFF)
    return 0;  // Outside UCS
  else if ((ucs4 >= 0xD800) && (ucs4 <= 0xDFFF))
    return 0;  // Surrogate

  if (ucs4 < 0x80)
  {
    (*utf8)[0] = (uint8_t) ucs4;
    return 1;
  }
  else if (ucs4 < 0x0800)
  {
    (*utf8)[0] = (uint8_t) (0xC0 | (ucs4 >> 6));
    (*utf8)[1] = (uint8_t) (0x80 | (ucs4 & 0x3F));
    return 2;
  }
  else if (ucs4 < 0x10000)
  {
    (*utf8)[0] = (uint8_t) (0xE0 | (ucs4 >> 12));
    (*utf8)[1] = (uint8_t) (0x80 | ((ucs4 >> 6) & 0x3F));
    (*utf8)[2] = (uint8_t) (0x80 | (ucs4 & 0x3F));
    return 3;
  }
  else
  {
    (*utf8)[0] = (uint8_t) (0xF0 | (ucs4 >> 18));
    (*utf8)[1] = (uint8_t) (0x80 | ((ucs4 >> 12) & 0x3F));
    (*utf8)[2] = (uint8_t) (0x80 | ((ucs4 >> 6) & 0x3F));
    (*utf8)[3] = (uint8_t) (0x80 | (ucs4 & 0x3F));
    return 4;
  }
}
