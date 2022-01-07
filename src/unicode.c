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

// Returns true if the given bytes are well-formed UTF-8.
bool is_utf8(const uint8_t *bytes, size_t length)
{
  for (int i = 0; i < length; i++)
  {
    int cont;  // Number of continuation bytes expected
    uint32_t ucs4;

    uint8_t b = bytes[i];
    if (b <= 0x7F)
      continue;  // ASCII
    else if (b <= 0xBF)
      return false;  // Continuation byte without lead byte
    else if (b <= 0xC1)
      return false;  // C0 through C1 unused
    else if (b <= 0xDF)
    {
      ucs4 = b & 0x1F;
      cont = 1;
    }
    else if (b <= 0xEF)
    {
      ucs4 = b & 0x0F;
      cont = 2;
    }
    else if (b <= 0xF4)
    {
      ucs4 = b & 0x07;
      cont = 3;
    }
    else
      return false;  // F5 through FF unused

    if (i + cont >= length)
      return false;  // Expected number of continuation bytes exceeds length

    // Read the continuation bytes
    for (int c = 0; c < cont; c++)
    {
      i++;
      b = bytes[i];
      if ((b < 0x80) || (b > 0xBF))
        return false;  // Not a continuation byte

      ucs4 = (ucs4 << 6) | (b & 0x3F);
    }

    // Check code point validity
    if ((ucs4 >= 0xD800) && (ucs4 <= 0xDFFF))
      return false;  // Surrogates
    else if (ucs4 < 0x80)
      return false;  // Overlong
    else if ((ucs4 >= 0x80) && (ucs4 <= 0x07FF) && (cont != 1))
      return false;  // Overlong
    else if ((ucs4 >= 0x0800) && (ucs4 <= 0xFFFF) && (cont != 2))
      return false;  // Overlong
    else if (ucs4 > 0x10FFFF)
      return false;  // Outside UCS
  }

  return true;
}
