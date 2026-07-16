#include "LTOS/lib/kprintf.h"
#include "LTOS/panic.hpp"
#include <cstdint>

extern "C" void divide_error() { panic::halt("Divide by zero"); }

extern "C" void unhandled_interrupt(uint64_t vector, uint64_t addr) {
  kprintf("vector=%d\n", vector);

  if (vector == 14) {
    kprintf("\r");
    kprintf("Page fault address: %x\n", addr);
    panic::halt("Page fault   ^");
  }

  panic::halt("^ Unhandled interrupt vector");
}
