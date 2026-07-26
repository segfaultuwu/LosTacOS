#pragma once

#include <stddef.h>

extern void putc(char c);
extern void puts(const char *str);
extern int printf(const char *fmt, ...);

extern int getchar(void);
extern int scanf(const char *fmt, ...);

extern int snprintf(char *buf, size_t size, const char *fmt, ...);
