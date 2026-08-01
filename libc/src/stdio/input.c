#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

static char input_buf[128];
static int input_pos = 0;
static int input_len = 0;

static int getchar_internal() {
  if (input_pos >= input_len) {
    long r = syscall(SYS_READ, 0, (long)input_buf, sizeof(input_buf));

    if (r <= 0)
      return -1;

    input_len = r;
    input_pos = 0;
  }

  return (unsigned char)input_buf[input_pos++];
}

char *fgets(char *str, int size, FILE *stream) {
  if (!str || !stream || size <= 0)
    return NULL;

  int i = 0;

  while (i < size - 1) {

    char c;

    long ret = read(stream->fd, &c, 1);

    if (ret <= 0) {
      if (ret == 0)
        stream->eof = 1;
      else
        stream->error = 1;

      break;
    }

    str[i++] = c;

    if (c == '\n')
      break;
  }

  if (i == 0)
    return NULL;

  str[i] = '\0';

  return str;
}

int getchar(void) {
  return getchar_internal();
}

int fgetc(FILE *stream) {
  if (!stream)
    return EOF;
  char c;
  long ret = read(stream->fd, &c, 1);
  if (ret <= 0) {
    if (ret == 0)
      stream->eof = 1;
    else
      stream->error = 1;
    return EOF;
  }
  return (unsigned char)c;
}

int getc(FILE *stream) {
  return fgetc(stream);
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!ptr || !stream || size == 0 || nmemb == 0)
    return 0;
  size_t total = size * nmemb;
  long ret = read(stream->fd, ptr, total);
  if (ret <= 0) {
    if (ret == 0)
      stream->eof = 1;
    else
      stream->error = 1;
    return 0;
  }
  return (size_t)(ret / size);
}

int scanf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);

  int count = 0;

  while (*fmt) {
    if (*fmt != '%') {
      fmt++;
      continue;
    }

    fmt++;

    // Parse an optional field width, e.g. the "127" in "%127s". Without
    // this, fmt points at '1' here, which matches neither 's' nor 'd'
    // below, so the conversion was silently skipped entirely -- no
    // getchar_internal() call ever happened, cmd was never touched, and
    // the shell's while(1) loop just spun printing "los> " as fast as
    // possible forever. This also lets %s actually respect the width so
    // a long line can't overflow the caller's buffer.
    int width = 0;

    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt++;
    }

    if (*fmt == 's') {
      char *buf = va_arg(args, char *);

      int i = 0;

      int c;

      // pomiń spacje i newline
      do {
        c = getchar_internal();
      } while (c == ' ' || c == '\n');

      while (c != ' ' && c != '\n' && c != -1) {
        if (width == 0 || i < width)
          buf[i++] = c;

        c = getchar_internal();
      }

      buf[i] = 0;

      count++;
    }

    else if (*fmt == 'd') {
      int *out = va_arg(args, int *);

      int value = 0;
      int sign = 1;

      int c;

      do {
        c = getchar_internal();
      } while (c == ' ' || c == '\n');

      if (c == '-') {
        sign = -1;
        c = getchar_internal();
      }

      while (c >= '0' && c <= '9') {
        value *= 10;
        value += c - '0';

        c = getchar_internal();
      }

      *out = value * sign;

      count++;
    }

    fmt++;
  }

  va_end(args);

  return count;
}

int vsscanf(const char *str, const char *fmt, va_list args) {
  if (!str || !fmt)
    return 0;

  const char *p = str;
  int count = 0;

  while (*fmt) {
    if (*fmt != '%') {
      if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') {
        while (*p == ' ' || *p == '\t' || *p == '\n')
          p++;
      } else if (*p == *fmt) {
        p++;
      } else {
        break;
      }
      fmt++;
      continue;
    }

    fmt++;

    int is_long = 0;
    int is_long_long = 0;

    int width = 0;
    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt++;
    }

    if (*fmt == 'l') {
      is_long = 1;
      fmt++;
      if (*fmt == 'l') {
        is_long_long = 1;
        fmt++;
      }
    } else if (*fmt == 'h') {
      fmt++;
      if (*fmt == 'h')
        fmt++;
    }

    if (*fmt == 's') {
      char *buf = va_arg(args, char *);
      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

      if (*p == '\0')
        break;

      int i = 0;
      while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        if (width == 0 || i < width)
          buf[i++] = *p;
        p++;
      }
      buf[i] = '\0';
      count++;
    } else if (*fmt == 'd' || *fmt == 'i' || *fmt == 'u') {
      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

      if (*p == '\0')
        break;

      int sign = 1;
      if (*p == '-') {
        sign = -1;
        p++;
      } else if (*p == '+') {
        p++;
      }

      if (*p < '0' || *p > '9')
        break;

      unsigned long long val = 0;
      int read_digits = 0;
      while (*p >= '0' && *p <= '9') {
        if (width > 0 && read_digits >= width)
          break;
        val = val * 10 + (*p - '0');
        p++;
        read_digits++;
      }

      if (is_long_long) {
        long long *out = va_arg(args, long long *);
        *out = (long long)val * sign;
      } else if (is_long) {
        long *out = va_arg(args, long *);
        *out = (long)val * sign;
      } else {
        int *out = va_arg(args, int *);
        *out = (int)val * sign;
      }

      count++;
    } else if (*fmt == 'x' || *fmt == 'X') {
      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

      if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;

      unsigned long long val = 0;
      int read_digits = 0;
      while (1) {
        if (width > 0 && read_digits >= width)
          break;
        char c = *p;
        int digit = -1;
        if (c >= '0' && c <= '9')
          digit = c - '0';
        else if (c >= 'a' && c <= 'f')
          digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
          digit = c - 'A' + 10;
        else
          break;

        val = (val << 4) | digit;
        p++;
        read_digits++;
      }

      if (read_digits == 0)
        break;

      if (is_long_long) {
        unsigned long long *out = va_arg(args, unsigned long long *);
        *out = val;
      } else if (is_long) {
        unsigned long *out = va_arg(args, unsigned long *);
        *out = (unsigned long)val;
      } else {
        unsigned int *out = va_arg(args, unsigned int *);
        *out = (unsigned int)val;
      }

      count++;
    } else if (*fmt == 'c') {
      char *out = va_arg(args, char *);
      if (*p == '\0')
        break;
      *out = *p++;
      count++;
    } else if (*fmt == '%') {
      if (*p == '%')
        p++;
      else
        break;
    }

    if (*fmt)
      fmt++;
  }

  return count;
}

int sscanf(const char *str, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int count = vsscanf(str, fmt, args);
  va_end(args);
  return count;
}
