#include <limits.h>
#include <stdlib.h>

int atoi(const char *str) {
  return (int)strtol(str, NULL, 10);
}

long atol(const char *str) {
  return strtol(str, NULL, 10);
}

long strtol(const char *str, char **endptr, int base) {
  const char *p = str;

  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
    p++;
  }

  int negative = 0;

  if (*p == '-') {
    negative = 1;
    p++;
  } else if (*p == '+') {
    p++;
  }

  if (base == 0) {
    if (p[0] == '0') {
      if (p[1] == 'x' || p[1] == 'X') {
        base = 16;
        p += 2;
      } else {
        base = 8;
      }
    } else {
      base = 10;
    }
  } else if (base == 16) {
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
      p += 2;
    }
  }

  long result = 0;

  while (*p) {
    int digit;

    if (*p >= '0' && *p <= '9')
      digit = *p - '0';

    else if (*p >= 'a' && *p <= 'z')
      digit = *p - 'a' + 10;

    else if (*p >= 'A' && *p <= 'Z')
      digit = *p - 'A' + 10;

    else
      break;

    if (digit >= base)
      break;

    result = result * base + digit;

    p++;
  }

  if (endptr)
    *endptr = (char *)p;

  if (negative)
    result = -result;

  return result;
}
