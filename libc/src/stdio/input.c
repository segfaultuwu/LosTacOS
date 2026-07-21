#include <stdarg.h>
#include <sys/syscall.h>

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

int getchar(void) {
  return getchar_internal();
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

      int c;

      // pomiń spacje i newline
      do {
        c = getchar_internal();
      } while (c == ' ' || c == '\n');

      while (c != ' ' && c != '\n' && c != -1) {
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
