#include "LTOS/arch/x86_64/sse.hpp"
#include <stdint.h>

void sse_init() {
  uint64_t cr0;
  uint64_t cr4;

  asm volatile("mov %%cr0, %0" : "=r"(cr0));

  cr0 &= ~(1ULL << 2); // EM = 0
  cr0 |= (1ULL << 1);  // MP = 1

  asm volatile("mov %0, %%cr0" ::"r"(cr0));

  asm volatile("clts");

  asm volatile("mov %%cr4, %0" : "=r"(cr4));

  cr4 |= (1ULL << 9);  // OSFXSR
  cr4 |= (1ULL << 10); // OSXMMEXCPT

  asm volatile("mov %0, %%cr4" ::"r"(cr4));
}
