#pragma once

namespace logger {

static volatile bool logger_lock = false;

void info(const char *fmt, ...);
void error(const char *fmt, ...);
void warn(const char *fmt, ...);
void test(const char *fmt, ...);
} // namespace logger
