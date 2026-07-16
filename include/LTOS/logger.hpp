#pragma once

namespace logger {
void info(const char *fmt, ...);
void error(const char *fmt, ...);
void warn(const char *fmt, ...);
void test(const char *fmt, ...);
} // namespace logger
