#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/kernel.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include <string.h>

using page_t = uint64_t;

namespace paging {

alignas(4096) uint64_t kernel_pml4[512];

static uint64_t next_free;
static uint64_t reserved_end = 0;

void reserve_below(uint64_t addr) {
  if (addr > reserved_end)
    reserved_end = addr;
}

PageTable *PageTable::create() {
  PageTable *table = (PageTable *)heap::kmalloc(sizeof(PageTable));

  if (!table)
    return nullptr;

  table->pml4 = create_address_space();

  if (!table->pml4) {
    heap::kfree(table);
    return nullptr;
  }

  table->phys = (uint64_t)table->pml4;

  return table;
}

bool PageTable::map(uint64_t virt, uint64_t phys, uint64_t flags) {
  map_page(pml4, virt, phys, flags);

  return true;
}

bool PageTable::unmap(uint64_t virt) {
  unmap_page(pml4, virt);

  return true;
}

void PageTable::activate() {
  asm volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

uint64_t PageTable::virt_to_phys(uint64_t virt) {
  uint64_t pml4_i = (virt >> 39) & 0x1FF;
  uint64_t pdpt_i = (virt >> 30) & 0x1FF;
  uint64_t pd_i = (virt >> 21) & 0x1FF;
  uint64_t pt_i = (virt >> 12) & 0x1FF;

  uint64_t *pdpt = (uint64_t *)(this->pml4[pml4_i] & ~0xFFF);
  if (!pdpt)
    return 0;

  uint64_t *pd = (uint64_t *)(pdpt[pdpt_i] & ~0xFFF);
  if (!pd)
    return 0;

  uint64_t *pt = (uint64_t *)(pd[pd_i] & ~0xFFF);
  if (!pt)
    return 0;

  return pt[pt_i] & ~0xFFF;
}

void *alloc_page() {

  uint64_t addr = next_free;

  next_free += 0x1000;

  memset((void *)addr, 0, 0x1000);

  return (void *)addr;
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

  uint64_t upper_flags = PAGE_PRESENT | PAGE_WRITABLE;
  if (flags & PAGE_USER)
    upper_flags |= PAGE_USER;

  // PML4
  if (!(pml4[pml4_i] & PAGE_PRESENT)) {
    pdpt = (uint64_t *)alloc_page();
    memset(pdpt, 0, 0x1000);
    pml4[pml4_i] = (uint64_t)pdpt | upper_flags;
  } else {
    pdpt = (uint64_t *)(pml4[pml4_i] & ~0xFFF);
    pml4[pml4_i] |= upper_flags;
  }

  // PDPT
  if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
    pd = (uint64_t *)alloc_page();
    memset(pd, 0, 0x1000);
    pdpt[pdpt_i] = (uint64_t)pd | upper_flags;
  } else {
    pd = (uint64_t *)(pdpt[pdpt_i] & ~0xFFF);
    pdpt[pdpt_i] |= upper_flags;
  }

  // PD
  if (!(pd[pd_i] & PAGE_PRESENT)) {
    pt = (uint64_t *)alloc_page();
    memset(pt, 0, 0x1000);
    pd[pd_i] = (uint64_t)pt | upper_flags;
  } else {
    pt = (uint64_t *)(pd[pd_i] & ~0xFFF);
    pd[pd_i] |= upper_flags;
  }

  // PTE
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

  uint64_t fb_addr = (uint64_t)framebuffer::get_address();
  uint64_t fb_size = framebuffer::get_pitch() * framebuffer::get_height();

  logger::info("Mapping framebuffer");

  for (uint64_t addr = fb_addr; addr < fb_addr + fb_size; addr += 0x1000) {

    map_page(kernel_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
  }

  logger::info("Framebuffer mapped");

  asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

  logger::info("CR3 loaded");

  uint64_t cr4;
  asm volatile("mov %%cr4,%0" : "=r"(cr4));

  cr4 |= (1 << 5); // PAE
  asm volatile("mov %0,%%cr4" : : "r"(cr4));

  uint64_t cr0;
  asm volatile("mov %%cr0,%0" : "=r"(cr0));

  cr0 |= (1ULL << 31); // PG
  asm volatile("mov %0,%%cr0" : : "r"(cr0));

  logger::info("Paging enabled");
}

uint64_t *create_address_space() {
  uint64_t *pml4 = (uint64_t *)alloc_page();
  memset(pml4, 0, 0x1000);

  // Higher half: keep sharing directly with the kernel table (unused by
  // anything today, but preserved for whenever it is).
  for (int i = 256; i < 512; i++) {
    pml4[i] = kernel_pml4[i];
  }

  // pml4[0] covers the entire low <512GB range, split into 1GB PDPT
  // slots. Only ONE of those slots is meant to be private per process:
  // slot 1 (va [1GB, 2GB), i.e. base 0x40000000), which is where
  // elf::load() places every process's binary. Every other slot --
  // the low identity map (heap, kernel image, etc., in slot 0), the
  // framebuffer (wherever QEMU happens to put it -- often several GB
  // in, i.e. a completely different slot), and anything else the
  // kernel maps into its own table -- needs to stay shared, or a
  // process faults the moment it touches something like the console.
  constexpr int USER_PDPT_SLOT = 0;

  uint64_t *pdpt = (uint64_t *)alloc_page();
  uint64_t *kernel_pdpt = (uint64_t *)(kernel_pml4[0] & ~0xFFFULL);

  if (kernel_pdpt) {
    for (int i = 0; i < 512; i++)
      pdpt[i] = kernel_pdpt[i];
  }

  pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE;

  return pml4;
}

void clone_user_pages(uint64_t *dst_pml4, uint64_t *src_pml4) {
  for (int pml4i = 0; pml4i < 256; pml4i++) {
    if (!(src_pml4[pml4i] & PAGE_PRESENT))
      continue;

    uint64_t *src_pdpt = (uint64_t *)(src_pml4[pml4i] & ~0xFFFULL);
    if (!src_pdpt)
      continue;

    for (int pi = 0; pi < 512; pi++) {
      if (!(src_pdpt[pi] & PAGE_PRESENT))
        continue;

      uint64_t *src_pd = (uint64_t *)(src_pdpt[pi] & ~0xFFFULL);

      for (int di = 0; di < 512; di++) {
        if (!(src_pd[di] & PAGE_PRESENT))
          continue;

        uint64_t *src_pt = (uint64_t *)(src_pd[di] & ~0xFFFULL);

        for (int ti = 0; ti < 512; ti++) {
          if (!(src_pt[ti] & PAGE_PRESENT))
            continue;

          uint64_t flags = src_pt[ti] & 0xFFF;
          if (!(flags & PAGE_USER))
            continue;

          uint64_t va = ((uint64_t)pml4i << 39) | ((uint64_t)pi << 30) | ((uint64_t)di << 21) | ((uint64_t)ti << 12);
          uint64_t src_phys = src_pt[ti] & ~0xFFFULL;

          void *dst_page = alloc_page();
          if (!dst_page)
            continue;

          uint64_t *src = (uint64_t *)(src_phys);
          uint64_t *dst = (uint64_t *)dst_page;

          for (int w = 0; w < 512; w++)
            dst[w] = src[w];

          map_page(dst_pml4, va, (uint64_t)dst_page, flags);
        }
      }
    }
  }
}

PageTable *clone_kernel_table() {
  return PageTable::create();
}

void switch_page_table(PageTable *table) {
  if (!table)
    return;

  asm volatile("mov %0, %%cr3" : : "r"(table->phys) : "memory");
}

} // namespace paging
