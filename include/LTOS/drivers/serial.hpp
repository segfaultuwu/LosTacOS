#pragma once

#include <stdarg.h>
#include <stdint.h>

namespace drivers::serial {

void init();

void outb(uint16_t port, uint8_t value);

uint8_t inb(uint16_t port);

void write(char c);

void write(const char *str);
void writef(const char *fmt, ...);
void vwritef(const char *fmt, va_list args);

} // namespace drivers::serial
