#include <stdarg.h>

extern long syscall(long num, long a, long b, long c);

#define SYS_READ 2

int getchar(void) {
  char c;

  long r = syscall(SYS_READ, 0, (long)&c, 1);

  if (r <= 0)
    return -1;

  return (unsigned char)c;
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

    if (*fmt == 's') {
      char *buf = va_arg(args, char *);

      int i = 0;

      char c;

      while (1) {
        c = getchar();

        if (c == '\n' || c == ' ')
          break;

        buf[i++] = c;
      }

      buf[i] = 0;

      count++;
    }

    else if (*fmt == 'd') {
      int *out = va_arg(args, int *);

      int value = 0;
      int sign = 1;

      char c = getchar();

      if (c == '-') {
        sign = -1;
        c = getchar();
      }

      while (c >= '0' && c <= '9') {
        value *= 10;
        value += c - '0';

        c = getchar();
      }

      *out = value * sign;

      count++;
    }

    fmt++;
  }

  va_end(args);

  return count;
}
