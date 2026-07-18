#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include <cstdarg>

namespace drivers::serial {

constexpr uint16_t COM1 = 0x3F8;

void outb(uint16_t port, uint8_t value) {
  asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
  uint8_t value;

  asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));

  return value;
}

void init() {
  outb(COM1 + 1, 0x00); // disable interrupts

  outb(COM1 + 3, 0x80); // enable DLAB

  outb(COM1 + 0, 0x03); // divisor low
  outb(COM1 + 1, 0x00); // divisor high

  outb(COM1 + 3, 0x03); // 8N1

  outb(COM1 + 2, 0xC7); // FIFO

  outb(COM1 + 4, 0x0B); // modem control
}

static bool can_write() { return inb(COM1 + 5) & 0x20; }

void write(char c) {
  while (!can_write())
    asm volatile("pause");

  outb(COM1, c);
}

void write(const char *str) {
  while (*str) {
    write(*str++);
  }
}

void writef(const char *fmt, ...) {
  char buffer[512];

  va_list args;
  va_start(args, fmt);

  kvsnprintf(buffer, sizeof(buffer), fmt, args);

  va_end(args);

  write(buffer);
}

} // namespace drivers::serial
