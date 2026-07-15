#include "LTOS/logger.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/timer.hpp"

#include <stdarg.h>

namespace logger {

enum Level { INFO = 1, WARN, ERROR };

static void log(int level, const char *fmt, va_list args) {
  uint64_t ms = timer::get_uptime_ms();

  uint64_t sec = ms / 1000;
  uint64_t frac = ms % 1000;

  kprintf("[ %d.%d ] ", sec, frac);
  switch (level) {
  case INFO:
    kprintf("\033[32m[INFO]\033[0m ");
    break;

  case WARN:
    kprintf("\033[33m[WARN]\033[0m ");
    break;

  case ERROR:
    kprintf("\033[31m[ERROR]\033[0m ");
    break;
  }

  kvprintf(fmt, args);

  kprintf("\n");
}

void info(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  log(INFO, fmt, args);
  va_end(args);
}

void warn(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  log(WARN, fmt, args);
  va_end(args);
}

void error(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  log(ERROR, fmt, args);
  va_end(args);
}

} // namespace logger
