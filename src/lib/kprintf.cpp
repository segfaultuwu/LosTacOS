#include "LTOS/lib/kprintf.h"
#include "LTOS/vga.hpp"

#include <stdarg.h>
#include <stdint.h>

static void print_char(char c) { vga::put(c); }

static int number_length(uint64_t value, int base) {
  int len = 1;

  while (value >= (uint64_t)base) {
    value /= base;
    len++;
  }

  return len;
}

static void print_number(uint64_t value, int base, int width = 0,
                         bool zero = false) {
  char buffer[32];

  const char *digits = "0123456789abcdef";

  int i = 0;

  if (value == 0) {
    buffer[i++] = '0';
  } else {
    while (value) {
      buffer[i++] = digits[value % base];
      value /= base;
    }
  }

  int padding = width - i;

  while (padding-- > 0)
    print_char(zero ? '0' : ' ');

  while (i--)
    print_char(buffer[i]);
}

static void print_string(const char *str, int width = 0, bool left = false) {
  if (!str)
    str = "(null)";

  int len = 0;

  const char *tmp = str;

  while (*tmp++)
    len++;

  if (!left) {
    for (int i = len; i < width; i++)
      print_char(' ');
  }

  while (*str)
    print_char(*str++);

  if (left) {
    for (int i = len; i < width; i++)
      print_char(' ');
  }
}

void kvprintf(const char *fmt, va_list args) {
  while (*fmt) {
    if (*fmt != '%') {
      print_char(*fmt++);
      continue;
    }

    fmt++;

    bool long_flag = false;
    bool left = false;
    bool zero = false;

    int width = 0;

    if (*fmt == '-') {
      left = true;
      fmt++;
    }

    if (*fmt == '0') {
      zero = true;
      fmt++;
    }

    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt++;
    }

    if (*fmt == 'l') {
      long_flag = true;
      fmt++;
    }

    switch (*fmt) {

    case 's': {
      const char *str = va_arg(args, const char *);
      print_string(str, width, left);
      break;
    }

    case 'd': {
      int64_t n;

      if (long_flag)
        n = va_arg(args, int64_t);
      else
        n = va_arg(args, int);

      if (n < 0) {
        print_char('-');
        n = -n;
      }

      print_number(n, 10, width, zero);
      break;
    }

    case 'u': {
      uint64_t n = va_arg(args, uint64_t);
      print_number(n, 10, width, zero);
      break;
    }

    case 'x': {
      uint64_t n = va_arg(args, uint64_t);
      print_number(n, 16, width, zero);
      break;
    }

    case '%':
      print_char('%');
      break;
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
