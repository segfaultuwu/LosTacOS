#include <strings.h>

int strcasecmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    char a = *s1;
    char b = *s2;

    if (a >= 'A' && a <= 'Z')
      a += 'a' - 'A';

    if (b >= 'A' && b <= 'Z')
      b += 'a' - 'A';

    if (a != b)
      return (unsigned char)a - (unsigned char)b;

    s1++;
    s2++;
  }

  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
  while (n && *s1 && *s2) {
    char a = *s1;
    char b = *s2;

    if (a >= 'A' && a <= 'Z')
      a += 'a' - 'A';

    if (b >= 'A' && b <= 'Z')
      b += 'a' - 'A';

    if (a != b)
      return (unsigned char)a - (unsigned char)b;

    s1++;
    s2++;
    n--;
  }

  if (!n)
    return 0;

  return (unsigned char)*s1 - (unsigned char)*s2;
}

void bzero(void *s, size_t n) {
  unsigned char *p = s;

  while (n--)
    *p++ = 0;
}

void bcopy(const void *src, void *dst, size_t n) {
  const unsigned char *s = src;
  unsigned char *d = dst;

  while (n--)
    *d++ = *s++;
}

int ffs(int i) {
  if (!i)
    return 0;

  int pos = 1;

  while (!(i & 1)) {
    i >>= 1;
    pos++;
  }

  return pos;
}
