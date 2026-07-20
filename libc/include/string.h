#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *memcpy(void *dest, const void *src, size_t n);

extern void *memset(void *dest, int value, size_t n);

extern void *memmove(volatile void *dest, const volatile void *src, size_t n);

extern int memcmp(const void *ptr1, const void *ptr2, size_t n);

extern size_t strlen(const char *str);

extern size_t strnlen(const char *str, size_t max);

extern char *strcpy(char *dest, const char *src);

extern char *strncpy(char *dest, const char *src, size_t n);

extern char *strcat(char *dest, const char *src);

extern int strcmp(const char *a, const char *b);

extern int strncmp(const char *a, const char *b, size_t n);

extern int strsplt(char *str, char *argv[], int max_args);

#ifdef __cplusplus
}
#endif
