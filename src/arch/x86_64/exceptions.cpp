#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/panic.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/scheduler.hpp"

#include <stdint.h>

extern "C" void divide_error() {
  sched::Task *curr = sched::get_current();
  if (curr && curr->process && curr->process->pid > 0) {
    sched::exit(136);
  }
  panic::halt("Divide by zero");
}

extern "C" void unhandled_interrupt(uint64_t vector, uint64_t err, uint64_t addr, uint64_t rip) {
  bool user = (err & 4) != 0;

  sched::Task *curr = sched::get_current();
  uint64_t pid = curr ? curr->pid : 0;

  logger::error("[TASK %lu] Exception vector=%lu err=%lx addr=%lx rip=%lx\n", pid, vector, err,
                addr, rip);

  if (user || (curr && curr->process && curr->process->pid > 0 && rip < 0x8000000000)) {
    logger::error("Terminating user process %lu due to exception %lu\n", pid, vector);
    sched::exit(128 + (int)vector);
  }

  if (vector == 14) {
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

    panic::halt("Kernel page fault");
  }

  panic::halt("^ Unhandled kernel exception");
}
