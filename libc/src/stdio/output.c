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

  write_buf(s, len);
  write_buf("\n", 1);

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

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
  size_t pos = 0;

  for (; *fmt; fmt++) {
    if (*fmt != '%') {
      buf_putc(buf, size, &pos, *fmt);
      continue;
    }

    fmt++;

    int left_align = 0;

    if (*fmt == '-') {
      left_align = 1;
      fmt++;
    }

    int width = 0;

    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt++;
    }

    switch (*fmt) {

    case 's': {
      const char *s = va_arg(args, const char *);
      if (!s)
        s = "(null)";

      size_t len = 0;
      while (s[len])
        len++;

      int pad = (width > (int)len) ? width - (int)len : 0;

      if (!left_align) {
        for (int i = 0; i < pad; i++)
          buf_putc(buf, size, &pos, ' ');
      }

      buf_puts(buf, size, &pos, s);

      if (left_align) {
        for (int i = 0; i < pad; i++)
          buf_putc(buf, size, &pos, ' ');
      }

      break;
    }

    case 'd': {
      int x = va_arg(args, int);
      if (x < 0) {
        buf_putc(buf, size, &pos, '-');
        x = -x;
      }
      buf_put_uint(buf, size, &pos, (unsigned long)x);
      break;
    }

    case 'u': {
      unsigned int x = va_arg(args, unsigned int);
      buf_put_uint(buf, size, &pos, x);
      break;
    }

    case 'c': {
      int c = va_arg(args, int);
      buf_putc(buf, size, &pos, (char)c);
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

  // Null-terminate
  if (size > 0) {
    if (pos < size)
      buf[pos] = '\0';
    else
      buf[size - 1] = '\0';
  }

  return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  int ret = vsnprintf(buf, size, fmt, args);

  va_end(args);

  return ret;
}

int printf(const char *fmt, ...) {
  char buf[1024];

  va_list args;
  va_start(args, fmt);

  int len = vsnprintf(buf, sizeof(buf), fmt, args);

  va_end(args);

  if (len > 0)
    write_buf(buf, len);

  return len;
}
