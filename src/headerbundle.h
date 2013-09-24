#ifndef MINISTOMPD_HEADERBUNDLE_H
#define MINISTOMPD_HEADERBUNDLE_H

struct header
{
  bytestring *key;
  bytestring *val;
};

typedef struct
{
  int            count;    // Number of headers stored
  int            size;     // Number of possible headers that fit in allocated memory
  struct header *headers;  // Array of headers
} headerbundle;

headerbundle     *headerbundle_new(void);
void              headerbundle_prepend_header(headerbundle *hb, bytestring *key, bytestring *val);
void              headerbundle_append_header(headerbundle *hb, bytestring *key, bytestring *val);
const bytestring *headerbundle_get_header_value_by_str(headerbundle *hb, const char *key);
void              headerbundle_dump(headerbundle *hb);
void              headerbundle_free(headerbundle *hb);

#endif
