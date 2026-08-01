#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *memcpy(void *dest, const void *src, size_t n);

extern void *memset(void *dest, int value, size_t n);

extern void *memmove(volatile void *dest, const volatile void *src, size_t n);

extern int memcmp(const void *ptr1, const void *ptr2, size_t n);

extern void *memchr(const void *s, int c, size_t n);

extern size_t strlen(const char *str);

extern size_t strnlen(const char *str, size_t max);

extern char *strcpy(char *dest, const char *src);

extern char *strncpy(char *dest, const char *src, size_t n);

extern char *strcat(char *dest, const char *src);

extern char *strncat(char *dest, const char *src, size_t n);

extern int strcmp(const char *a, const char *b);

extern int strncmp(const char *a, const char *b, size_t n);

extern int strcasecmp(const char *s1, const char *s2);

extern int strncasecmp(const char *s1, const char *s2, size_t n);

extern char *strchr(const char *s, int c);

extern char *strrchr(const char *s, int c);

extern char *strstr(const char *haystack, const char *needle);

extern int strsplt(char *str, char *argv[], int max_args);

#ifdef __cplusplus
}
#endif
