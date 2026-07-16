#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/kernel.hpp"
#include "LTOS/logger.hpp"

using page_t = uint64_t;

namespace paging {

static uint64_t next_free;
static uint64_t reserved_end = 0;

void reserve_below(uint64_t addr) {
  if (addr > reserved_end)
    reserved_end = addr;
}

void *alloc_page() {
  uint64_t addr = next_free;

  next_free += 0x1000;

  // for debugging
  // kprintf("alloc=%x\n", addr);

  uint64_t *p = (uint64_t *)addr;

  for (int i = 0; i < 512; i++)
    p[i] = 0;

  return p;
}

void init() {
  for (int i = 0; i < 512; i++)
    kernel_pml4[i] = 0;

  next_free = ((uint64_t)&_kernel_end + 0xFFF) & ~0xFFF;

  uint64_t reserved_aligned = (reserved_end + 0xFFF) & ~0xFFF;
  if (reserved_aligned > next_free)
    next_free = reserved_aligned;

  // Debug shit
  // kprintf("_kernel_end=%x\n", (uint64_t)&_kernel_end);
  // kprintf("next=%x\n", next_free);
}

void map_page(uint64_t *pml4, uint64_t va, uint64_t pa, uint64_t flags) {
  uint64_t pml4_i = (va >> 39) & 0x1FF;
  uint64_t pdpt_i = (va >> 30) & 0x1FF;
  uint64_t pd_i = (va >> 21) & 0x1FF;
  uint64_t pt_i = (va >> 12) & 0x1FF;

  uint64_t *pdpt;
  uint64_t *pd;
  uint64_t *pt;

  if (!(pml4[pml4_i] & PAGE_PRESENT)) {
    pdpt = (uint64_t *)alloc_page();

    pml4[pml4_i] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE;
  } else {
    pdpt = (uint64_t *)(pml4[pml4_i] & ~0xFFF);
  }

  if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
    pd = (uint64_t *)alloc_page();

    pdpt[pdpt_i] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITABLE;
  } else {
    pd = (uint64_t *)(pdpt[pdpt_i] & ~0xFFF);
  }

  if (!(pd[pd_i] & PAGE_PRESENT)) {
    pt = (uint64_t *)alloc_page();

    pd[pd_i] = (uint64_t)pt | PAGE_PRESENT | PAGE_WRITABLE;
  } else {
    pt = (uint64_t *)(pd[pd_i] & ~0xFFF);
  }

  pt[pt_i] = (pa & ~0xFFF) | flags;
}

void unmap_page(uint64_t *pml4, uint64_t va) {
  uint64_t pml4_i = (va >> 39) & 0x1FF;
  uint64_t pdpt_i = (va >> 30) & 0x1FF;
  uint64_t pd_i = (va >> 21) & 0x1FF;
  uint64_t pt_i = (va >> 12) & 0x1FF;

  if (!(pml4[pml4_i] & PAGE_PRESENT))
    return;

  uint64_t *pdpt = (uint64_t *)(pml4[pml4_i] & ~0xFFF);

  if (!(pdpt[pdpt_i] & PAGE_PRESENT))
    return;

  uint64_t *pd = (uint64_t *)(pdpt[pdpt_i] & ~0xFFF);

  if (!(pd[pd_i] & PAGE_PRESENT))
    return;

  uint64_t *pt = (uint64_t *)(pd[pd_i] & ~0xFFF);

  if (!(pt[pt_i] & PAGE_PRESENT))
    return;

  pt[pt_i] = 0;

  asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void setup_kernel_identity() {
  logger::info("Identity mapping");

  for (uint64_t addr = 0; addr < 0x10000000; addr += 0x1000) {
    map_page(kernel_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
  }

  // VGA
  map_page(kernel_pml4, 0xB8000, 0xB8000, PAGE_PRESENT | PAGE_WRITABLE);

  logger::info("Identity done");
}

void enable_paging() {
  uint64_t cr3 = (uint64_t)kernel_pml4;

  asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

  logger::info("CR3 loaded");

  uint64_t cr4;

  asm volatile("mov %%cr4,%0" : "=r"(cr4));

  cr4 |= (1 << 5);

  asm volatile("mov %0,%%cr4" : : "r"(cr4));

  uint64_t cr0;

  asm volatile("mov %%cr0,%0" : "=r"(cr0));

  cr0 |= (1ULL << 31);

  asm volatile("mov %0,%%cr0" : : "r"(cr0));
}

uint64_t *create_address_space() {
  uint64_t *pml4 = (uint64_t *)alloc_page();

  for (int i = 256; i < 512; i++) {
    pml4[i] = kernel_pml4[i];
  }

  return pml4;
}

} // namespace paging
