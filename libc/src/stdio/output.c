#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

static void write_buf(const char *buf, size_t len) {
  syscall(SYS_WRITE, 1, (long)buf, len);
}

int fputc(int c, FILE *stream) {
  if (!stream)
    return EOF;
  char ch = (char)c;
  long ret = write(stream->fd, &ch, 1);
  if (ret <= 0) {
    stream->error = 1;
    return EOF;
  }
  return (unsigned char)c;
}

int putc(int c, FILE *stream) {
  return fputc(c, stream);
}

int puts(const char *s) {
  size_t len = 0;

  while (s[len])
    len++;

  write_buf(s, len);
  write_buf("\n", 1);

  return 0;
}

int fputs(const char *s, FILE *stream) {
  if (!s || !stream)
    return EOF;
  size_t len = 0;
  while (s[len])
    len++;
  long ret = write(stream->fd, s, len);
  if (ret < 0) {
    stream->error = 1;
    return EOF;
  }
  return (int)ret;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!ptr || !stream || size == 0 || nmemb == 0)
    return 0;
  size_t total = size * nmemb;
  long ret = write(stream->fd, ptr, total);
  if (ret < 0) {
    stream->error = 1;
    return 0;
  }
  return (size_t)(ret / size);
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

static void buf_put_hex(char *buf, size_t size, size_t *pos, unsigned long x) {
  char tmp[32];
  const char *hex = "0123456789abcdef";

  int i = 0;

  if (x == 0) {
    buf_putc(buf, size, pos, '0');
    return;
  }

  while (x) {
    tmp[i++] = hex[x & 0xf];
    x >>= 4;
  }

  while (i--)
    buf_putc(buf, size, pos, tmp[i]);
}

static void buf_put_signed(char *buf, size_t size, size_t *pos, long x) {
  if (x < 0) {
    buf_putc(buf, size, pos, '-');
    x = -x;
  }

  buf_put_uint(buf, size, pos, (unsigned long)x);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
  size_t pos = 0;

  while (*fmt) {

    if (*fmt != '%') {
      buf_putc(buf, size, &pos, *fmt++);
      continue;
    }

    fmt++;

    if (*fmt == '%') {
      buf_putc(buf, size, &pos, '%');
      fmt++;
      continue;
    }

    int left = 0;

    if (*fmt == '-') {
      left = 1;
      fmt++;
    }

    int width = 0;

    while (*fmt >= '0' && *fmt <= '9') {

      width *= 10;
      width += *fmt - '0';

      fmt++;
    }

    int long_flag = 0;

    if (*fmt == 'l') {
      long_flag = 1;
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

      int pad = 0;

      if (width > (int)len)
        pad = width - (int)len;

      if (!left) {
        for (int i = 0; i < pad; i++)
          buf_putc(buf, size, &pos, ' ');
      }

      buf_puts(buf, size, &pos, s);

      if (left) {
        for (int i = 0; i < pad; i++)
          buf_putc(buf, size, &pos, ' ');
      }

      break;
    }

    case 'c': {
      int c = va_arg(args, int);

      buf_putc(buf, size, &pos, (char)c);

      break;
    }

    case 'd':
    case 'i': {
      long x;

      if (long_flag)
        x = va_arg(args, long);
      else
        x = va_arg(args, int);

      buf_put_signed(buf, size, &pos, x);

      break;
    }

    case 'u': {
      unsigned long x;

      if (long_flag)
        x = va_arg(args, unsigned long);
      else
        x = va_arg(args, unsigned int);

      buf_put_uint(buf, size, &pos, x);

      break;
    }

    case 'x': {
      unsigned long x;

      if (long_flag)
        x = va_arg(args, unsigned long);
      else
        x = va_arg(args, unsigned int);

      buf_put_hex(buf, size, &pos, x);

      break;
    }

    case 'p': {
      void *ptr = va_arg(args, void *);

      buf_puts(buf, size, &pos, "0x");

      buf_put_hex(buf, size, &pos, (unsigned long)ptr);

      break;
    }

    case 'z': {
      if (*fmt == 'u') {

        size_t x = va_arg(args, size_t);

        buf_put_uint(buf, size, &pos, x);

        fmt++;
      }

      break;
    }

    default:

      buf_putc(buf, size, &pos, '%');

      if (*fmt)
        buf_putc(buf, size, &pos, *fmt);

      break;
    }

    fmt++;
  }

  if (size) {

    if (pos < size)
      buf[pos] = 0;
    else
      buf[size - 1] = 0;
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
    write_buf(buf, (size_t)len);

  return len;
}
