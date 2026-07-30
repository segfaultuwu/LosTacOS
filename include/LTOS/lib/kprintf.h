#pragma once

#include <stdarg.h>
#include <stddef.h>

void kprintf(const char *format, ...);
void kvprintf(const char *fmt, va_list args);
int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
