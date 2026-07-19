#include "string.h"
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

  while (n >= 8) {
    *d64++ = *s64++;
    n -= 8;
  }

  d = (uint8_t *)d64;
  s = (const uint8_t *)s64;

  while (n) {
    *d++ = *s++;
    n--;
  }

  return dest;
}

void *memset(void *dest, int value, size_t count) {
  unsigned char *ptr = (unsigned char *)dest;

  while (count--)
    *ptr++ = (unsigned char)value;

  return dest;
}

void *memmove(volatile void *dest, const volatile void *src, size_t n) {
  volatile unsigned char *d = (unsigned char *)dest;
  const volatile unsigned char *s = (unsigned char *)src;

  if (d < s) {
    for (size_t i = 0; i < n; i++)
      d[i] = s[i];
  } else {
    for (size_t i = n; i > 0; i--)
      d[i - 1] = s[i - 1];
  }

  return (void *)dest;
}

int memcmp(const void *ptr1, const void *ptr2, size_t n) {
  const unsigned char *a = (unsigned char *)ptr1;
  const unsigned char *b = (unsigned char *)ptr2;

  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i])
      return a[i] - b[i];
  }

  return 0;
}

size_t strlen(const char *str) {
  size_t len = 0;

  while (str[len])
    len++;

  return len;
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

  return (unsigned char)*a - (unsigned char)*b;
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
}
