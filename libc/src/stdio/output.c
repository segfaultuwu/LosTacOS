#include <stdarg.h>
#include <stddef.h>
#include <sys/syscall.h>

static void write_buf(const char *buf, size_t len) {
  syscall(SYS_WRITE, 1, (long)buf, len);
}

int putc(int c) {
  char ch = c;

  write_buf(&ch, 1);

  return c;
}

int puts(const char *s) {
  size_t len = 0;

  while (s[len])
    len++;

  syscall(SYS_WRITE, 1, (long)s, len);

  syscall(SYS_WRITE, 1, (long)"\n", 1);

  return 0;
}

static void print_uint(unsigned long x) {
  char buf[32];
  int i = 0;

  if (x == 0) {
    putc('0');
    return;
  }

  while (x) {
    buf[i++] = '0' + (x % 10);
    x /= 10;
  }

  while (i--)
    putc(buf[i]);
}

static void buf_putc(char *buf, size_t size, size_t *pos, char c) {
  if (*pos + 1 < size)
    buf[*pos] = c;

  (*pos)++;
}

static void buf_puts(char *buf, size_t size, size_t *pos, const char *s) {
  while (*s)
    buf_putc(buf, size, pos, *s++);
}

static void buf_put_uint(char *buf, size_t size, size_t *pos, unsigned long x) {
  char tmp[32];
  int i = 0;

  if (x == 0) {
    buf_putc(buf, size, pos, '0');
    return;
  }

  while (x) {
    tmp[i++] = '0' + (x % 10);
    x /= 10;
  }

  while (i--)
    buf_putc(buf, size, pos, tmp[i]);
}

int printf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);

  for (; *fmt; fmt++) {
    if (*fmt != '%') {
      putc(*fmt);
      continue;
    }

    fmt++;

    switch (*fmt) {

    case 's': {
      char *s = va_arg(args, char *);

      while (*s)
        putc(*s++);

      break;
    }

    case 'd': {
      int x = va_arg(args, int);

      if (x < 0) {
        putc('-');
        x = -x;
      }

      print_uint(x);

      break;
    }

    case 'u': {
      unsigned int x = va_arg(args, unsigned int);

      print_uint(x);

      break;
    }

    case 'c': {
      int c = va_arg(args, int);

      putc(c);

      break;
    }

    case '%':
      putc('%');
      break;

    default:
      putc('%');
      putc(*fmt);
      break;
    }
  }

  va_end(args);

  return 0;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);

  size_t pos = 0;

  for (; *fmt; fmt++) {

    if (*fmt != '%') {
      buf_putc(buf, size, &pos, *fmt);
      continue;
    }

    fmt++;

    switch (*fmt) {

    case 's': {
      char *s = va_arg(args, char *);

      buf_puts(buf, size, &pos, s);

      break;
    }

    case 'd': {
      int x = va_arg(args, int);

      if (x < 0) {
        buf_putc(buf, size, &pos, '-');
        x = -x;
      }

      buf_put_uint(buf, size, &pos, x);

      break;
    }

    case 'u': {
      unsigned int x = va_arg(args, unsigned int);

      buf_put_uint(buf, size, &pos, x);

      break;
    }

    case 'c': {
      int c = va_arg(args, int);

      buf_putc(buf, size, &pos, c);

      break;
    }

    case '%':
      buf_putc(buf, size, &pos, '%');
      break;

    default:
      buf_putc(buf, size, &pos, '%');
      buf_putc(buf, size, &pos, *fmt);
      break;
    }
  }

  if (size > 0) {
    if (pos >= size)
      buf[size - 1] = 0;
    else
      buf[pos] = 0;
  }

  va_end(args);

  return pos;
}
