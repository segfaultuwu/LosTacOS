#include "LTOS/lib/kprintf.h"
#include "LTOS/vga.hpp"
#include <stdarg.h>
#include <stdint.h>

static void print_number(uint64_t value, int base) {
  char buffer[32];

  const char *digits = "0123456789abcdef";

  int i = 0;

  if (value == 0) {
    vga::put('0');
    return;
  }

  while (value) {
    buffer[i++] = digits[value % base];
    value /= base;
  }

  while (i--) {
    vga::put(buffer[i]);
  }
}

void kvprintf(const char *fmt, va_list args) {
  while (*fmt) {
    if (*fmt != '%') {
      vga::put(*fmt++);
      continue;
    }

    fmt++;

    bool long_flag = false;

    if (*fmt == 'l') {
      long_flag = true;
      fmt++;
    }

    switch (*fmt) {

    case 's': {
      const char *str = va_arg(args, const char *);

      if (!str)
        str = "(null)";

      while (*str)
        vga::put(*str++);

      break;
    }

    case 'd': {
      if (long_flag) {
        int64_t n = va_arg(args, int64_t);
        print_number(n, 10);
      } else {
        int n = va_arg(args, int);
        print_number(n, 10);
      }

      break;
    }

    case 'u': {
      uint64_t n = va_arg(args, uint64_t);
      print_number(n, 10);
      break;
    }

    case 'x': {
      uint64_t n = va_arg(args, uint64_t);
      print_number(n, 16);
      break;
    }

    case '%': {
      vga::put('%');
      break;
    }
    }

    fmt++;
  }
}

void kprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  kvprintf(fmt, args);

  va_end(args);
}
