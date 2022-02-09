#include <stdbool.h>
#include <stdint.h>

// NOTE: Unsigned integer overflow wraps without causing undefined behaviour.
#define MULTIPLY_UNSIGNED_SAFE(name, type) \
bool multiply_ ## name ## _safe(type *out, type m1, type m2) \
{ \
  type result = m1 * m2; \
  if ((m1 > 0) && ((result / m1) != m2)) \
    return false; \
\
  *out = result; \
  return true; \
}

MULTIPLY_UNSIGNED_SAFE(uint32, uint32_t)
MULTIPLY_UNSIGNED_SAFE(uint64, uint64_t)
