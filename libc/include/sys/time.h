#pragma once

#include <stddef.h>
#include <stdint.h>

typedef long suseconds_t;
typedef long time_t;

struct timeval {
  time_t tv_sec;
  suseconds_t tv_usec;
};

struct timezone {
  int tz_minuteswest;
  int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
