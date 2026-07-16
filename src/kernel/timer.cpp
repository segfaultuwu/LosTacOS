#include "LTOS/timer.hpp"
#include "LTOS/drivers/serial.hpp"

namespace timer {

static volatile uint64_t counter = 0;
static uint32_t frequency = 100;

void init(uint32_t freq) {
  uint32_t divisor = 1193182 / freq;

  drivers::serial::outb(0x43, 0x36);

  drivers::serial::outb(0x40, divisor & 0xff);
  drivers::serial::outb(0x40, (divisor >> 8) & 0xff);
}

uint64_t get_uptime_ms() {
  // ticks / freq = seconds
  // *1000 = miliseconds
  return (counter * 1000) / frequency;
}

uint64_t get_uptime_sec() { return counter / frequency; }

void tick() { counter++; }

uint64_t ticks() { return counter; }

void sleep(uint64_t ms) {
  uint64_t start = counter;

  uint64_t wait_ticks = (ms * frequency) / 1000;

  while (counter - start < wait_ticks) {
    asm volatile("hlt");
  }
}

} // namespace timer
