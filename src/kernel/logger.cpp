#include "LTOS/logger.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/lib/kprintf.h"

#include <cstdint>
#include <stdarg.h>

namespace logger {

enum Level { INFO = 1, WARN, ERROR, TEST };

// TODO: fix undefined behaviour when printing with format specifiers
void log(Level level, const char *fmt, va_list args) {
  uint64_t sec = timer::get_uptime_ms() / 1000;
  uint64_t frac = timer::get_uptime_ms() % 1000;
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
  case TEST:
    kprintf("\033[34m[TEST]\033[0m ");
  }

  va_list copy;
  va_copy(copy, args);

  kvprintf(fmt, copy);

  va_end(copy);

  kprintf("\n");
}
void info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log(INFO, fmt, args);

  va_end(args);
}

void test(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log(TEST, fmt, args);

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
