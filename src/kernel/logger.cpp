#include "LTOS/logger.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/lib/kprintf.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

namespace logger {

enum Level { INFO = 1, WARN, ERROR, TEST };

static char klog_ring[32768];
static size_t klog_write_head = 0;

static void klog_append_char(char c) {
  klog_ring[klog_write_head % sizeof(klog_ring)] = c;
  klog_write_head++;
}

static void klog_append_str(const char *str) {
  if (!str)
    return;
  while (*str) {
    klog_append_char(*str++);
  }
}

size_t read_klog(char *out, size_t max_len, size_t offset) {
  size_t total = klog_write_head > sizeof(klog_ring) ? sizeof(klog_ring) : klog_write_head;
  size_t start = klog_write_head > sizeof(klog_ring) ? (klog_write_head % sizeof(klog_ring)) : 0;

  if (offset >= total)
    return 0;

  size_t avail = total - offset;
  size_t n = max_len < avail ? max_len : avail;

  for (size_t i = 0; i < n; i++) {
    size_t idx = (start + offset + i) % sizeof(klog_ring);
    out[i] = klog_ring[idx];
  }

  return n;
}

void log(Level level, const char *fmt, va_list args) {
  uint32_t ms = (uint32_t)timer::get_uptime_ms();
  uint32_t sec = ms / 1000;
  uint32_t frac = (ms % 1000) / 10;

  char header_buf[64];
  ksnprintf(header_buf, sizeof(header_buf), "[ %u.%02u ] ", sec, frac);


  const char *tag = "[INFO] ";
  if (level == WARN)
    tag = "[WARN] ";
  else if (level == ERROR)
    tag = "[ERROR] ";
  else if (level == TEST)
    tag = "[TEST] ";

  char msg_buf[512];
  va_list copy;
  va_copy(copy, args);
  kvsnprintf(msg_buf, sizeof(msg_buf), fmt, copy);
  va_end(copy);

  // Append to kernel log ring buffer (clean text without ANSI colors)
  klog_append_str(header_buf);
  klog_append_str(tag);
  klog_append_str(msg_buf);
  klog_append_char('\n');

  // Write to console / serial output in a single formatted line
  const char *tag_color = "\033[32m[INFO]\033[0m ";
  if (level == WARN)
    tag_color = "\033[33m[WARN]\033[0m ";
  else if (level == ERROR)
    tag_color = "\033[31m[ERROR]\033[0m ";
  else if (level == TEST)
    tag_color = "\033[34m[TEST]\033[0m ";

  char line_buf[1024];
  ksnprintf(line_buf, sizeof(line_buf), "%s%s%s\n", header_buf, tag_color, msg_buf);
  kprintf("%s", line_buf);
}

void info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log(INFO, fmt, args);

  va_end(args);
}

void test(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log(TEST, fmt, args);

  va_end(args);
}

void warn(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log(WARN, fmt, args);

  va_end(args);
}

void error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  log(ERROR, fmt, args);

  va_end(args);
}

} // namespace logger
