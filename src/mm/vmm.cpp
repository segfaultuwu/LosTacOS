#include "LTOS/mm/vmm.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"

namespace vmm {

static uint64_t *kernel_pml4 = nullptr;

void init(uint64_t *pml4) {
  kernel_pml4 = pml4;
}

void map(uintptr_t virt, uintptr_t phys, uint64_t flags) {
  virt &= ~0xFFF;
  phys &= ~0xFFF;

  paging::map_page(kernel_pml4, virt, phys, flags);
}

void unmap(uintptr_t virt) {
  paging::unmap_page(kernel_pml4, virt & ~0xFFF);
}

uintptr_t alloc(size_t size, uint64_t flags) {
  size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  // tymczasowo stały VA
  static uintptr_t next = 0xFFFF800000000000;

  uintptr_t start = next;

  for (uintptr_t addr = start; addr < start + size; addr += PAGE_SIZE) {
    uintptr_t page = pmm::alloc_page();

    if (!page)
      return 0;

    map(addr, page, flags);
  }

  next += size;

  return start;
}

} // namespace vmm
