#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/panic.hpp"
#include "LTOS/sched/scheduler.hpp"

#include <cstdint>

extern "C" void divide_error() {
  panic::halt("Divide by zero");
}

void handle_user_page_fault(uint64_t addr, uint64_t rip, uint64_t err) {
  logger::error("[TASK %d] SEGFAULT at %x (rip=%x)\n", sched::get_current()->pid, addr, rip);

  sched::exit();
}

extern "C" void unhandled_interrupt(uint64_t vector, uint64_t err, uint64_t addr, uint64_t rip) {
  bool present = err & 1;
  bool write = err & 2;
  bool user = err & 4;

  logger::error("vector=%d rip=%x err/addr=%x", vector, rip, addr);

  switch (vector) {
  case 14: {

    bool user = err & 4;

    logger::error("vector=%lx err=%lx addr=%lx rip=%lx", vector, err, addr, rip);
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t *pml4 = (uint64_t *)(cr3 & ~0xfff);
    uint64_t pml4e = pml4[(addr >> 39) & 0x1ff];
    uint64_t *pdpt = (uint64_t *)(pml4e & ~0xfff);
    uint64_t pdpte = pdpt[(addr >> 30) & 0x1ff];
    uint64_t *pd = (uint64_t *)(pdpte & ~0xfff);
    uint64_t pde = pd[(addr >> 21) & 0x1ff];
    uint64_t *pt = (uint64_t *)(pde & ~0xfff);
    uint64_t pte = pt[(addr >> 12) & 0x1ff];

    logger::error("PML4=%lx PDPT=%lx PD=%lx PTE=%lx", pml4e, pdpte, pde, pte);

    if (user)
      handle_user_page_fault(addr, rip, err);

    panic::halt("Kernel page fault");
  }
  case 6: {
    logger::error("unhandled interrupt vector: 6 (#UD - invalid opcode)");
    logger::error("rip=%x", rip);

    uint8_t *code = (uint8_t *)rip;
    logger::error("bytes: ");
    for (int i = 0; i < 8; i++)
      kprintf("%x ", code[i]);
    kprintf("\n");

    panic::halt("Invalid opcode");
    break;
  }
  }

  panic::halt("^ Unhandled interrupt vector");
}
