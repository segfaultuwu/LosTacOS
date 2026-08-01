#pragma once
#include <stddef.h>

namespace logger {

static volatile bool logger_lock = false;

void info(const char *fmt, ...);
void error(const char *fmt, ...);
void warn(const char *fmt, ...);
void test(const char *fmt, ...);

size_t read_klog(char *out, size_t max_len, size_t offset);
} // namespace logger
