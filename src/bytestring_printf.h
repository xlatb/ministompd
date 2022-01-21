#ifndef MINISTOMPD_BYTESTRING_PRINTF_H
#define MINISTOMPD_BYTESTRING_PRINTF_H

bytestring *bytestring_printf(bytestring *pb, const char *fmt, ...);
bytestring *bytestring_vprintf(bytestring *pb, const char *fmt, va_list args);

#endif
