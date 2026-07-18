#include "LTOS/lib/kprintf.h"
#include "LTOS/panic.hpp"
#include <cstdint>

extern "C" void divide_error() { panic::halt("Divide by zero"); }

extern "C" void unhandled_interrupt(uint64_t vector, uint64_t addr,
                                    uint64_t rip) {
  kprintf("vector=%d\n", vector);

  switch (vector) {
  case 14:
    kprintf("\r");
    kprintf("Page fault address: %x\n", addr);
    panic::halt("Page fault   ^");
    break;

  case 6: {
    kprintf("unhandled interrupt vector: 6 (#UD - invalid opcode)\n");
    kprintf("rip=%x\n", rip);

    uint8_t *code = (uint8_t *)rip;
    kprintf("bytes: ");
    for (int i = 0; i < 8; i++)
      kprintf("%x ", code[i]);
    kprintf("\n");

    panic::halt("Invalid opcode");
    break;
  }
  }

  panic::halt("^ Unhandled interrupt vector");
}
