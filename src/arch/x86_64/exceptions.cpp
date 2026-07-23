#include "LTOS/lib/kprintf.h"
#include "LTOS/panic.hpp"
#include "LTOS/sched/scheduler.hpp"

#include <cstdint>

extern "C" void divide_error() {
  panic::halt("Divide by zero");
}

void handle_user_page_fault(uint64_t addr, uint64_t rip, uint64_t err) {
  kprintf("[TASK %d] SEGFAULT at %x (rip=%x)\n", sched::get_current()->pid, addr, rip);

  sched::exit();
}

extern "C" void unhandled_interrupt(uint64_t vector, uint64_t err, uint64_t addr, uint64_t rip) {
  bool present = err & 1;
  bool write = err & 2;
  bool user = err & 4;

  kprintf("vector=%d rip=%x err/addr=%x\n", vector, rip, addr);

  switch (vector) {
  case 14: {

    bool user = err & 4;

    kprintf("vector=%lx err=%lx addr=%lx rip=%lx\n", vector, err, addr, rip);

    if (user)
      sched::exit();

    panic::halt("Kernel page fault");
  }
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
