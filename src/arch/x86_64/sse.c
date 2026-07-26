#include <stdint.h>

void sse_init() {
  uint64_t cr0;
  uint64_t cr4;

  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));

  // disable FPU emulation
  cr0 &= ~(1ULL << 2); // CR0.EM = 0

  // monitore FPU
  cr0 |= (1ULL << 1); // CR0.MP = 1

  __asm__ volatile("mov %0, %%cr0" ::"r"(cr0));

  __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

  // enable SSE
  cr4 |= (1ULL << 9);  // OSFXSR
  cr4 |= (1ULL << 10); // OSXMMEXCPT

  __asm__ volatile("mov %0, %%cr4" ::"r"(cr4));
}
