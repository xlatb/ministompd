#include "ministompd.h"
#include "tomlvalue.h"
#include "list.h"
#include "hash.h"

#include <stdio.h>
#include <inttypes.h>  // PRId64
#include <string.h>
#include <assert.h>
#include <time.h>

static bool is_leap_year(int year)
{
  if (year % 4)
    return false;
  else if (year % 100)
    return true;
  else if (year % 400)
     return false;
  else
     return true;
}

static int days_in_month(int year, int month)
{
  static const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if ((month < 1) || (month > 12))
    return 0;

  if (month != 2)
    return days_per_month[month - 1];

  return 28 + (is_leap_year(year) ? 1 : 0);
}

static int64_t pack_time(int year, int month, int day, int hour, int minute, int second, int msec)
{
  // Normalize months
  while (month < 1)  { month += 12; year--; }
  while (month > 12) { month -= 12; year++; }

  // Find day count at start of target year
  int daycount = 0;
  if (year < TOML_TIME_EPOCH_YEAR)
  {
    // Step backwards by years
    for (int y = TOML_TIME_EPOCH_YEAR; y > year; y--)
      daycount -= (is_leap_year(y - 1) ? 366 : 365);
  }
  else
  {
    // Step forwards by years
    for (int y = TOML_TIME_EPOCH_YEAR; y < year; y++)
      daycount += (is_leap_year(y) ? 366 : 365);
  }

  // Step forwards by months
  for (int m = 1; m < month; m++)
    daycount += days_in_month(year, m);

  // Step forwards by days
  daycount += (day - 1);

  int64_t v = (daycount * TOML_TIME_DAY) + (hour * TOML_TIME_HOUR) + (minute * TOML_TIME_MINUTE) + (second * TOML_TIME_SECOND) + msec;
  return v;
}

static void unpack_time(int64_t v, int *yearptr, int *monthptr, int *dayptr, int *hourptr, int *minuteptr, int *secondptr, int *msecptr)
{
  bool negative = v < 0;

  int daycount = v / TOML_TIME_DAY;

  // Find date from daycount
  int monthdays;
  int year, month, day;
  if (!negative)
  {
    year = TOML_TIME_EPOCH_YEAR;
    month = 1;
    day = 1;

    while (daycount > (monthdays = days_in_month(year, month)))
    {
      daycount -= monthdays;
      month++;
      day = 1;
      if (month > 12)
      {
        month = 1;
        year += 1;
      }
    }

    day += daycount;
  }
  else
  {
    year = TOML_TIME_EPOCH_YEAR - 1;
    month = 12;
    day = 31;

    daycount = abs(daycount);
    while (daycount > (monthdays = days_in_month(year, month)))
    {
      daycount -= monthdays;
      month -= 1;
      if (month == 0)
      {
        month = 12;
        year -= 1;
      }
      day = days_in_month(year, month);
    }

    day -= daycount;
  }

  v = ((v % TOML_TIME_DAY) + TOML_TIME_DAY) % TOML_TIME_DAY;  // Remove date component

  int hour, minute, second, msec;

  msec = v % TOML_TIME_SECOND;
  v /= TOML_TIME_SECOND;

  second = v % 60;
  v /= 60;

  minute = v % 60;
  v /= 60;

  hour = v % 24;
  v /= 24;

  if (yearptr)   *yearptr   = year;
  if (monthptr)  *monthptr  = month;
  if (dayptr)    *dayptr    = day;
  if (hourptr)   *hourptr   = hour;
  if (minuteptr) *minuteptr = minute;
  if (secondptr) *secondptr = second;
  if (msecptr)   *msecptr   = msec;

  return;
}

void tomlvalue_free(tomlvalue *v)
{
  switch (v->type)
  {
  case TOML_TYPE_NONE:
  case TOML_TYPE_INT:
  case TOML_TYPE_FLOAT:
  case TOML_TYPE_BOOL:
  case TOML_TYPE_TIME:
  case TOML_TYPE_DATE:
  case TOML_TYPE_DATETIME:
  case TOML_TYPE_DATETIME_TZ:
    xfree(v);
    break;
  case TOML_TYPE_STR:
   {
     bytestring_free(v->u.strval);
     xfree(v);
   }
   break;
  case TOML_TYPE_ARRAY:
    {
      tomlvalue *e;
      while ((e = list_pop(v->u.arrayval)))
        tomlvalue_free(e);
      list_free(v->u.arrayval);
      xfree(v);
    }
    break;
  case TOML_TYPE_TABLE:
    {
      tomlvalue *e;
      while ((e = hash_remove_any(v->u.tableval, NULL)))
        tomlvalue_free(e);
      hash_free(v->u.tableval);
      xfree(v);
    }
    break;
  default:
    abort();
  }
}

void tomlvalue_dump(tomlvalue *v, int indent)
{
  for (int i = 0; i < indent; i++) printf("  ");

  switch (v->type)
  {
  case TOML_TYPE_NONE:
    printf("(none)");
    break;
  case TOML_TYPE_STR:
    {
      printf("STR length %zu value '", bytestring_get_length(v->u.strval));
      fwrite(bytestring_get_bytes(v->u.strval), bytestring_get_length(v->u.strval), 1, stdout);
      printf("'\n");
    }
    break;
  case TOML_TYPE_INT:
    printf("INT %" PRId64 "\n", v->u.intval);
    break;
  case TOML_TYPE_FLOAT:
    printf("FLOAT %f %e %a\n", v->u.floatval, v->u.floatval, v->u.floatval);
    break;
  case TOML_TYPE_BOOL:
    printf("BOOL %s\n", (v->u.boolval) ? "true" : "false");
    break;
  case TOML_TYPE_TIME:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("TIME %02d:%02d:%02d.%0*d (%" PRId64 ")\n", unpacked.hour, unpacked.minute, unpacked.second, TOML_TIME_FRAC_DIGITS, unpacked.msec, (int64_t) v->u.datetimeval.msecs);
    }
    break;
  case TOML_TYPE_DATE:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("DATE %04d-%02d-%02d (%" PRId64 ")\n", unpacked.year, unpacked.month, unpacked.day, (int64_t) v->u.datetimeval.msecs);
    }
    break;
  case TOML_TYPE_DATETIME:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("DATETIME %04d-%02d-%02d %02d:%02d:%02d.%0*d (%" PRId64 ")\n",
       unpacked.year, unpacked.month, unpacked.day,
       unpacked.hour, unpacked.minute, unpacked.second, TOML_TIME_FRAC_DIGITS, unpacked.msec,
       (int64_t) v->u.datetimeval.msecs);
    }
    break;
  case TOML_TYPE_DATETIME_TZ:
    {
      struct tomlvalue_unpacked_datetime unpacked;
      tomlvalue_get_datetime(v, &unpacked);
      printf("DATETIME_TZ %04d-%02d-%02d %02d:%02d:%02d.%0*d%c%02d:%02d (%" PRId64 ")\n",
       unpacked.year, unpacked.month, unpacked.day,
       unpacked.hour, unpacked.minute, unpacked.second, TOML_TIME_FRAC_DIGITS, unpacked.msec,
       unpacked.tzneg ? '-' : '+', abs(unpacked.tzminutes) / 60, abs(unpacked.tzminutes) % 60,
       (int64_t) v->u.datetimeval.msecs);
    }
    break;
  case TOML_TYPE_ARRAY:
    {
      int len = list_get_length(v->u.arrayval);
      printf("ARRAY length %d\n", len);
      for (int i = 0; i < len; i++)
      {
        tomlvalue *kid = list_get_item(v->u.arrayval, i);
        tomlvalue_dump(kid, indent + 1);
      }
    }
    break;
  case TOML_TYPE_TABLE:
    {
      int count = hash_get_itemcount(v->u.tableval);

      const bytestring *keys[count];
      hash_get_keys(v->u.tableval, keys, count);

      printf("TABLE item count %d\n", count);
      for (int i = 0; i < count; i++)
      {
        for (int j = 0; j < indent; j++) printf("  ");
        printf("item %d key '", i);
        fwrite(bytestring_get_bytes(keys[i]), bytestring_get_length(keys[i]), 1, stdout);
        printf("':\n");

        tomlvalue *kid = hash_get(v->u.tableval, keys[i]);
        tomlvalue_dump(kid, indent + 1);
      }
    }
    break;
  default:
    printf("UNKNOWN TYPE %d\n", v->type);
  }
}

tomlvalue *tomlvalue_new(void)
{
  tomlvalue *v = xmalloc(sizeof(struct tomlvalue));
  v->type = TOML_TYPE_NONE;
  v->flags = 0;
  return v;
}

tomlvalue *tomlvalue_new_table(void)
{
  struct tomlvalue *v = xmalloc(sizeof(struct tomlvalue));
  v->type = TOML_TYPE_TABLE;
  v->flags = 0;
  v->u.tableval = hash_new(8);
  return v;
}

tomlvalue *tomlvalue_new_array(void)
{
  struct tomlvalue *v = xmalloc(sizeof(struct tomlvalue));
  v->type = TOML_TYPE_ARRAY;
  v->flags = 0;
  v->u.arrayval = list_new(8);
  return v;
}

const char *tomlvalue_unpacked_datetime_check(struct tomlvalue_unpacked_datetime *unpacked)
{
  if ((unpacked->year < 1) || (unpacked->year > 9999))
    return "Year out of range 1..9999";
  else if ((unpacked->month < 1) || (unpacked->month > 12))
    return "Month out of range 1..12";
  else if ((unpacked->day < 1) || (unpacked->day > days_in_month(unpacked->year, unpacked->month)))
    return "Day of month out of range";
  else if ((unpacked->hour < 0) || (unpacked->hour > 23))
    return "Hour out of range 0..23";
  else if ((unpacked->minute < 0) || (unpacked->minute > 59))
    return "Minute out of range 0..59";
  else if ((unpacked->second < 0) || (unpacked->second > 60))  // Allow for 60th second (leap second)
    return "Second out of range 0..60";
  else if ((unpacked->msec < 0) || (unpacked->msec > 999))
    return "Milliseconds out of range 0..999";

  return NULL;
}

bool tomlvalue_get_datetime(tomlvalue *v, struct tomlvalue_unpacked_datetime *unpacked)
{
  if ((v->type & TOML_TYPESET_MASK) != TOML_TYPESET_DATETIME) return false;  // Ensure a datetime type

  unpacked->tzneg = v->u.datetimeval.tzsign;
  unpacked->tzminutes = ((v->u.datetimeval.tzsign) ? -1 : 1) * v->u.datetimeval.tzminutes;

  int64_t msecs = v->u.datetimeval.msecs;
  msecs += (unpacked->tzminutes * TOML_TIME_MINUTE);  // Adjust to stored timezone
  unpack_time(msecs, &unpacked->year, &unpacked->month, &unpacked->day, &unpacked->hour, &unpacked->minute, &unpacked->second, &unpacked->msec);

  return true;
}

bool tomlvalue_set_datetime(tomlvalue *v, struct tomlvalue_unpacked_datetime *unpacked)
{
  if ((v->type & TOML_TYPESET_MASK) != TOML_TYPESET_DATETIME) return false;  // Ensure a datetime type

  bool tzneg = unpacked->tzneg || (unpacked->tzminutes < 0);
  int  tzminutes = abs(unpacked->tzminutes);

  v->u.datetimeval.tzsign = (tzneg) ? 1 : 0;
  v->u.datetimeval.tzminutes = tzminutes;

  int64_t packed = pack_time(unpacked->year, unpacked->month, unpacked->day, unpacked->hour, unpacked->minute, unpacked->second, unpacked->msec);
  packed -= (tzneg ? -1 : 1) * (tzminutes * TOML_TIME_MINUTE);  // Adjust to UTC
  v->u.datetimeval.msecs = packed;

  return true;
}

bool tomlvalue_get_tzoffset(tomlvalue *v, bool *negative, int *minutes)
{
  if ((v->type & TOML_TYPESET_MASK) != TOML_TYPESET_DATETIME) return false;  // Ensure a datetime type

  if (negative)
    *negative = v->u.datetimeval.tzsign ? true : false;

  if (minutes)
    *minutes  = v->u.datetimeval.tzminutes * ((v->u.datetimeval.tzsign) ? -1 : 1);

  return true;
}

void tomlvalue_set_flag(tomlvalue *v, int flag, bool value)
{
  v->flags = (v->flags & ~flag) | (value ? flag : 0);
}

bool tomlvalue_get_flag(tomlvalue *v, int flag)
{
  return (v->flags & flag) != 0;
}
