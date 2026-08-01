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

int putchar(int c) {
  return fputc(c, stdout);
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

static void buf_put_padded_num(char *buf, size_t size, size_t *pos, unsigned long x, int is_negative, int width, int zero_pad, int left, int is_hex) {
  char tmp[64];
  int i = 0;
  const char *hex_chars = "0123456789abcdef";

  if (x == 0) {
    tmp[i++] = '0';
  } else {
    while (x) {
      if (is_hex) {
        tmp[i++] = hex_chars[x & 0xf];
        x >>= 4;
      } else {
        tmp[i++] = '0' + (x % 10);
        x /= 10;
      }
    }
  }

  int total_len = i + (is_negative ? 1 : 0);
  int pad = (width > total_len) ? (width - total_len) : 0;

  if (is_negative && zero_pad) {
    buf_putc(buf, size, pos, '-');
  }

  if (!left) {
    char pad_char = zero_pad ? '0' : ' ';
    for (int p = 0; p < pad; p++) {
      buf_putc(buf, size, pos, pad_char);
    }
  }

  if (is_negative && !zero_pad) {
    buf_putc(buf, size, pos, '-');
  }

  while (i--) {
    buf_putc(buf, size, pos, tmp[i]);
  }

  if (left) {
    for (int p = 0; p < pad; p++) {
      buf_putc(buf, size, pos, ' ');
    }
  }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
  size_t pos = 0;

  while (*fmt) {
    if (*fmt != '%') {
      buf_putc(buf, size, &pos, *fmt++);
      continue;
    }

    fmt++; // Skip '%'

    if (*fmt == '%') {
      buf_putc(buf, size, &pos, '%');
      fmt++;
      continue;
    }

    int left = 0;
    int zero_pad = 0;

    if (*fmt == '-') {
      left = 1;
      fmt++;
    } else if (*fmt == '0') {
      zero_pad = 1;
      fmt++;
    }

    int width = 0;
    if (*fmt == '*') {
      width = va_arg(args, int);
      if (width < 0) {
        left = 1;
        width = -width;
      }
      fmt++;
    } else {
      while (*fmt >= '0' && *fmt <= '9') {
        width = width * 10 + (*fmt - '0');
        fmt++;
      }
    }

    int precision = -1;
    if (*fmt == '.') {
      fmt++;
      if (*fmt == '*') {
        precision = va_arg(args, int);
        fmt++;
      } else {
        precision = 0;
        while (*fmt >= '0' && *fmt <= '9') {
          precision = precision * 10 + (*fmt - '0');
          fmt++;
        }
      }
    }

    int long_flag = 0;
    if (*fmt == 'l') {
      long_flag = 1;
      fmt++;
    } else if (*fmt == 'z') {
      long_flag = 1;
      fmt++;
    }

    switch (*fmt) {
    case 's': {
      const char *s = va_arg(args, const char *);
      if (!s)
        s = "(null)";

      size_t len = 0;
      while (s[len] && (precision < 0 || (int)len < precision))
        len++;

      int pad = (width > (int)len) ? (width - (int)len) : 0;

      if (!left) {
        for (int i = 0; i < pad; i++)
          buf_putc(buf, size, &pos, ' ');
      }

      for (size_t i = 0; i < len; i++)
        buf_putc(buf, size, &pos, s[i]);

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
      long x = long_flag ? va_arg(args, long) : va_arg(args, int);
      int is_neg = 0;
      unsigned long uval;
      if (x < 0) {
        is_neg = 1;
        uval = (unsigned long)(-x);
      } else {
        uval = (unsigned long)x;
      }
      buf_put_padded_num(buf, size, &pos, uval, is_neg, width, zero_pad, left, 0);
      break;
    }

    case 'u': {
      unsigned long x = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
      buf_put_padded_num(buf, size, &pos, x, 0, width, zero_pad, left, 0);
      break;
    }

    case 'x': {
      unsigned long x = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
      buf_put_padded_num(buf, size, &pos, x, 0, width, zero_pad, left, 1);
      break;
    }

    case 'p': {
      void *ptr = va_arg(args, void *);
      buf_puts(buf, size, &pos, "0x");
      buf_put_padded_num(buf, size, &pos, (unsigned long)ptr, 0, width, zero_pad, left, 1);
      break;
    }

    default:
      buf_putc(buf, size, &pos, '%');
      if (*fmt)
        buf_putc(buf, size, &pos, *fmt);
      break;
    }

    if (*fmt)
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
