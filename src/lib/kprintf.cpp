#include "LTOS/lib/kprintf.h"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"

#include <stdarg.h>
#include <stdint.h>

#include "LTOS/lib/kprintf.h"
#include <stdarg.h>
#include <stdint.h>

struct SnBuf {
  char *buf;
  size_t size;
  size_t pos;
};

static void sn_putc(SnBuf &sb, char c) {
  if (sb.pos + 1 < sb.size)
    sb.buf[sb.pos] = c;

  sb.pos++;
}

static void sn_puts(SnBuf &sb, const char *s) {
  while (*s)
    sn_putc(sb, *s++);
}

static int sn_number_length(uint64_t value, int base) {
  int len = 1;
  while (value >= (uint64_t)base) {
    value /= base;
    len++;
  }
  return len;
}

static void sn_print_number(SnBuf &sb, uint64_t value, int base, int width = 0, bool zero = false) {
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
    sn_putc(sb, zero ? '0' : ' ');

  while (i--)
    sn_putc(sb, buffer[i]);
}

static void sn_print_string(SnBuf &sb, const char *str, int width = 0, bool left = false) {
  if (!str)
    str = "(null)";

  int len = 0;
  const char *tmp = str;
  while (*tmp++)
    len++;

  if (!left) {
    for (int i = len; i < width; i++)
      sn_putc(sb, ' ');
  }

  while (*str)
    sn_putc(sb, *str++);

  if (left) {
    for (int i = len; i < width; i++)
      sn_putc(sb, ' ');
  }
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
  SnBuf sb{buf, size, 0};

  while (*fmt) {
    if (*fmt != '%') {
      sn_putc(sb, *fmt++);
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
      sn_print_string(sb, str, width, left);
      break;
    }
    case 'd': {
      int64_t n;
      if (long_flag)
        n = va_arg(args, int64_t);
      else
        n = va_arg(args, int);

      if (n < 0) {
        sn_putc(sb, '-');
        n = -n;
      }

      sn_print_number(sb, n, 10, width, zero);
      break;
    }
    case 'u': {
      uint64_t n = va_arg(args, uint64_t);
      sn_print_number(sb, n, 10, width, zero);
      break;
    }
    case 'x': {
      uint64_t n = va_arg(args, uint64_t);
      sn_print_number(sb, n, 16, width, zero);
      break;
    }
    case '%':
      sn_putc(sb, '%');
      break;
    default:
      sn_putc(sb, '%');
      if (*fmt)
        sn_putc(sb, *fmt);
      break;
    }

    if (*fmt)
      fmt++;
  }

  if (sb.size > 0) {
    size_t term = sb.pos < sb.size ? sb.pos : sb.size - 1;
    sb.buf[term] = '\0';
  }

  return (int)sb.pos;
}

static void print_char(char c) {
  console::put(c);
}

static int number_length(uint64_t value, int base) {
  int len = 1;

  while (value >= (uint64_t)base) {
    value /= base;
    len++;
  }

  return len;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);

  int ret = kvsnprintf(buf, size, fmt, args);

  va_end(args);

  return ret;
}

static void print_number(uint64_t value, int base, int width = 0, bool zero = false) {
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
  framebuffer::swap();
}

void kvprintf(const char *fmt, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  drivers::serial::vwritef(fmt, args_copy);
  va_end(args_copy);

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

  framebuffer::swap();

  va_end(args);
}
