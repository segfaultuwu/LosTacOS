#pragma once

#include <cstdarg>
#include <cstddef>

void kprintf(const char *format, ...);
void kvprintf(const char *fmt, va_list args);
int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);
