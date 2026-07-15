#include "LTOS/timer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"

namespace timer {

static volatile uint64_t tick_count = 0;

void init(uint32_t frequency) {
  uint32_t divisor = 1193182 / frequency;

  drivers::serial::outb(0x43, 0x36);

  drivers::serial::outb(0x40, divisor & 0xff);

  drivers::serial::outb(0x40, (divisor >> 8) & 0xff);

  kprintf("PIT initialized: %d Hz", frequency);
}

void tick() { tick_count++; }

uint64_t ticks() { return tick_count; }

void sleep(uint64_t ms) {
  uint64_t target = tick_count + ms;

  while (tick_count < target) {
    asm volatile("hlt");
  }
}

} // namespace timer
