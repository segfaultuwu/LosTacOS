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
