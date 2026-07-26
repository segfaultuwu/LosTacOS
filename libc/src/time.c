#include <time.h>

time_t time(time_t *t) {
  time_t now = 0;

  if (t)
    *t = now;

  return now;
}

clock_t clock(void) {
  return 0;
}

struct tm *gmtime(const time_t *timep) {
  static struct tm tm;

  tm.tm_sec = 0;
  tm.tm_min = 0;
  tm.tm_hour = 0;
  tm.tm_mday = 1;
  tm.tm_mon = 0;
  tm.tm_year = 70;

  return &tm;
}

struct tm *localtime(const time_t *timep) {
  return gmtime(timep);
}

time_t mktime(struct tm *tm) {
  return 0;
}

char *asctime(const struct tm *tm) {
  return "Thu Jan 01 00:00:00 1970\n";
}

char *ctime(const time_t *timep) {
  return asctime(gmtime(timep));
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
  if (max)
    s[0] = 0;

  return 0;
}

double difftime(time_t end, time_t beginning) {
  return (double)(end - beginning);
}
