#ifndef MINISTOMPD_MATHUTILS_H
#define MINISTOMPD_MATHUTILS_H

#define MULTIPLY_UNSIGNED_SAFE(name, type) bool multiply_ ## name ## _safe(type *out, type m1, type m2);
MULTIPLY_UNSIGNED_SAFE(uint32, uint32_t)
MULTIPLY_UNSIGNED_SAFE(uint64, uint64_t)
#undef MULTIPLY_UNSIGNED_SAFE

#endif
