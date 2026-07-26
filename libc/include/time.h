#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>
#include <stdint.h>

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000000

#define NULL ((void *)0)

struct tm {
  int tm_sec;  // seconds 0-60
  int tm_min;  // minutes 0-59
  int tm_hour; // hours 0-23
  int tm_mday; // day of month 1-31
  int tm_mon;  // month 0-11
  int tm_year; // years since 1900
  int tm_wday; // day of week 0-6
  int tm_yday; // day of year 0-365
  int tm_isdst;
};

time_t time(time_t *t);

clock_t clock(void);

struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);

time_t mktime(struct tm *tm);

char *asctime(const struct tm *tm);
char *ctime(const time_t *timep);

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);

double difftime(time_t end, time_t beginning);

#endif
