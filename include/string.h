#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *dest, int value, size_t n);

void *memmove(volatile void *dest, const volatile void *src, size_t n);

int memcmp(const void *ptr1, const void *ptr2, size_t n);

size_t strlen(const char *str);

size_t strnlen(const char *str, size_t max);

char *strcpy(char *dest, const char *src);

char *strncpy(char *dest, const char *src, size_t n);

char *strcat(char *dest, const char *src);

int strcmp(const char *a, const char *b);

int strncmp(const char *a, const char *b, size_t n);

#ifdef __cplusplus
}
#endif
