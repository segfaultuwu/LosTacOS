#include "string.h"
#include "LTOS/mm/heap.hpp"
#include <stdint.h>

extern "C" {

void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;

  while (n && ((uintptr_t)d & 7)) {
    *d++ = *s++;
    n--;
  }

  uint64_t *d64 = (uint64_t *)d;
  const uint64_t *s64 = (const uint64_t *)s;

  while (n >= 64) {
    d64[0] = s64[0];
    d64[1] = s64[1];
    d64[2] = s64[2];
    d64[3] = s64[3];
    d64[4] = s64[4];
    d64[5] = s64[5];
    d64[6] = s64[6];
    d64[7] = s64[7];
    d64 += 8;
    s64 += 8;
    n -= 64;
  }

  while (n >= 8) {
    *d64++ = *s64++;
    n -= 8;
  }

  d = (uint8_t *)d64;
  s = (const uint8_t *)s64;

  while (n)
    *d++ = *s++, n--;

  return dest;
}

char *strdup(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)heap::kmalloc(len + 1);
  if (!out)
    return nullptr;

  memcpy(out, s, len);
  out[len] = 0;

  return out;
}

void *memset(void *dest, int value, size_t count) {
  uint8_t *d = (uint8_t *)dest;
  uint8_t v = (uint8_t)value;

  while (count && ((uintptr_t)d & 7)) {
    *d++ = v;
    count--;
  }

  uint64_t v64 =
      ((uint64_t)v << 56) | ((uint64_t)v << 48) | ((uint64_t)v << 40) | ((uint64_t)v << 32) |
      ((uint64_t)v << 24) | ((uint64_t)v << 16) | ((uint64_t)v << 8) | v;

  uint64_t *d64 = (uint64_t *)d;

  while (count >= 64) {
    d64[0] = v64;
    d64[1] = v64;
    d64[2] = v64;
    d64[3] = v64;
    d64[4] = v64;
    d64[5] = v64;
    d64[6] = v64;
    d64[7] = v64;
    d64 += 8;
    count -= 64;
  }

  while (count >= 8) {
    *d64++ = v64;
    count -= 8;
  }

  d = (uint8_t *)d64;
  while (count)
    *d++ = v, count--;

  return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (unsigned char *)src;

  if (d < s) {
    memcpy(dest, src, n);
  } else if (d > s) {
    while (n) {
      n--;
      d[n] = s[n];
    }
  }

  return dest;
}

int memcmp(const void *ptr1, const void *ptr2, size_t n) {
  const uint8_t *a = (const uint8_t *)ptr1;
  const uint8_t *b = (const uint8_t *)ptr2;

  while (n && ((uintptr_t)a & 7)) {
    if (*a != *b)
      return (int)*a - (int)*b;
    a++;
    b++;
    n--;
  }

  const uint64_t *a64 = (const uint64_t *)a;
  const uint64_t *b64 = (const uint64_t *)b;

  while (n >= 8) {
    if (*a64 != *b64) {
      a = (const uint8_t *)a64;
      b = (const uint8_t *)b64;
      for (size_t i = 0; i < 8; i++) {
        if (a[i] != b[i])
          return (int)a[i] - (int)b[i];
      }
    }
    a64++;
    b64++;
    n -= 8;
  }

  a = (const uint8_t *)a64;
  b = (const uint8_t *)b64;

  while (n) {
    if (*a != *b)
      return (int)*a - (int)*b;
    a++;
    b++;
    n--;
  }

  return 0;
}

static inline uint64_t has_zero(uint64_t v) {
  return (v - 0x0101010101010101ULL) & ~v & 0x8080808080808080ULL;
}

size_t strlen(const char *str) {
  const char *s = str;

  while ((uintptr_t)s & 7) {
    if (!*s)
      return (size_t)(s - str);
    s++;
  }

  const uint64_t *w = (const uint64_t *)s;
  while (!has_zero(*w))
    w++;

  s = (const char *)w;
  while (*s)
    s++;

  return (size_t)(s - str);
}

size_t strnlen(const char *str, size_t max) {
  size_t len = 0;

  while (len < max && str[len])
    len++;

  return len;
}

char *strcpy(char *dest, const char *src) {
  char *ret = dest;

  while ((*dest++ = *src++))
    ;

  return ret;
}

char *strncpy(char *dest, const char *src, size_t n) {
  size_t i;

  for (i = 0; i < n && src[i]; i++)
    dest[i] = src[i];

  for (; i < n; i++)
    dest[i] = 0;

  return dest;
}

char *strcat(char *dest, const char *src) {
  char *ret = dest;

  while (*dest)
    dest++;

  while ((*dest++ = *src++))
    ;

  return ret;
}

int strcmp(const char *a, const char *b) {
  while (*a && (*a == *b)) {
    a++;
    b++;
  }

  return *(unsigned char *)a - *(unsigned char *)b;
}

char *strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;

  for (; *haystack; haystack++) {
    if (*haystack == *needle) {
      const char *h = haystack;
      const char *n = needle;
      while (*h && *n && *h == *n) {
        h++;
        n++;
      }
      if (!*n)
        return (char *)haystack;
    }
  }

  return nullptr;
}

int strncmp(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i] || a[i] == '\0')
      return (unsigned char)a[i] - (unsigned char)b[i];
  }

  return 0;
}

int strsplt(char *str, char *argv[], int max_args) {
  int argc = 0;

  while (*str && argc < max_args) {
    while (*str == ' ')
      str++;

    if (*str == '\0')
      break;

    argv[argc++] = str;

    while (*str && *str != ' ')
      str++;

    if (*str) {
      *str = '\0';
      str++;
    }
  }

  return argc;
}

char *strchr(const char *s, int c) {
  while (*s) {
    if (*s == (char)c)
      return (char *)s;
    s++;
  }
  if (c == '\0')
    return (char *)s;
  return nullptr;
}

char *strrchr(const char *s, int c) {
  const char *last = nullptr;
  do {
    if (*s == (char)c)
      last = s;
  } while (*s++);
  return (char *)last;
}

char *strncat(char *dest, const char *src, size_t n) {
  char *ret = dest;
  while (*dest)
    dest++;
  while (n-- && *src)
    *dest++ = *src++;
  *dest = '\0';
  return ret;
}

void *memchr(const void *s, int c, size_t n) {
  const unsigned char *p = (const unsigned char *)s;
  while (n--) {
    if (*p == (unsigned char)c)
      return (void *)p;
    p++;
  }
  return nullptr;
}
}
