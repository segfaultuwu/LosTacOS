#include "LTOS/mm/paging.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/kernel.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include <string.h>

using page_t = uint64_t;

namespace paging {

alignas(4096) uint64_t kernel_pml4[512];

static uint64_t reserved_end = 0;

// Upper bound of the region identity-mapped by setup_kernel_identity().
// Every address space shares that mapping (via the copied low PDPT /
// higher-half PML4 entries in create_address_space()), so any page-table
// page or leaf page we hand out for direct pointer use has to live below
// this line -- anything pmm returns above it is not dereferenceable from
// wherever alloc_page() happens to be called.
//
// TODO: once there's a proper higher-half direct map (HHDM) of all
// physical memory, this restriction goes away and alloc_page() can use
// the full range pmm tracks.
constexpr uint64_t IDENTITY_MAP_LIMIT = 0x10000000; // 256MB, matches setup_kernel_identity()

void reserve_below(uint64_t addr) {
  if (addr <= reserved_end)
    return;

  uint64_t aligned = (addr + 0xFFF) & ~0xFFFULL;

  // This used to work by pushing the bump allocator's start past addr,
  // so nothing below it was ever handed out -- almost certainly how the
  // framebuffer (and anything else that isn't reliably marked reserved
  // in the multiboot memory map) was being protected. Now that
  // alloc_page() draws from pmm, that protection has to be applied to
  // pmm's bitmap directly, or pmm will treat this range as ordinary free
  // memory and hand pieces of it out.
  //
  // IMPORTANT: this only works correctly if pmm::init() has already run.
  // pmm::init() unconditionally frees every "available" multiboot region
  // via free_region(), which would silently undo a reservation applied
  // before it. If anything calls reserve_below() before pmm::init(),
  // this needs to be deferred (e.g. paging::init() re-applying
  // reserved_end after pmm::init() has run) rather than applied here.
  pmm::reserve_region(0, aligned);

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
  return map_page(pml4, virt, phys, flags);
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
  // Was a raw bump pointer that never checked bounds and could never
  // reclaim pages freed by destroy_user_pages()/pmm::free_page() -- every
  // fork() permanently burned memory that reap() could never give back,
  // and running off the end of real/identity-mapped RAM silently
  // corrupted state instead of failing. pmm is now the single source of
  // truth for both allocation and free-list reuse; we just have to keep
  // it inside the identity-mapped window (see IDENTITY_MAP_LIMIT) until
  // there's a proper HHDM.
  uintptr_t phys = pmm::alloc_page();

  if (!phys)
    return nullptr;

  if (phys >= IDENTITY_MAP_LIMIT) {
    // Not usable as a direct pointer from every address space. Hand it
    // back to pmm rather than leaking it, and fail cleanly -- callers
    // all check for null now.
    pmm::free_page(phys);
    return nullptr;
  }

  void *page = (void *)phys;
  memset(page, 0, 0x1000);
  return page;
}

void init() {
  for (int i = 0; i < 512; i++)
    kernel_pml4[i] = 0;

  // Safety net: if reserve_below() was called before pmm::init() ran
  // (and therefore got wiped out by pmm::init()'s free_region() calls),
  // re-apply it now. Harmless no-op if reserve_below() already applied
  // it successfully after pmm::init() -- reserve_region() is idempotent.
  if (reserved_end) {
    uint64_t aligned = (reserved_end + 0xFFF) & ~0xFFFULL;
    pmm::reserve_region(0, aligned);
  }
}

void destroy_user_pages(uint64_t *pml4) {
  for (int pml4i = 0; pml4i < 256; pml4i++) {
    uint64_t pml4e = pml4[pml4i];

    if (!(pml4e & PAGE_PRESENT))
      continue;

    uint64_t *pdpt = (uint64_t *)(pml4e & ~0xFFFULL);

    for (int pi = 0; pi < 512; pi++) {
      uint64_t pdpte = pdpt[pi];

      if (!(pdpte & PAGE_PRESENT))
        continue;

      uint64_t *pd = (uint64_t *)(pdpte & ~0xFFFULL);

      for (int di = 0; di < 512; di++) {
        uint64_t pde = pd[di];

        if (!(pde & PAGE_PRESENT))
          continue;

        uint64_t *pt = (uint64_t *)(pde & ~0xFFFULL);

        for (int ti = 0; ti < 512; ti++) {
          uint64_t pte = pt[ti];

          if (!(pte & PAGE_PRESENT))
            continue;

          uint64_t flags = pte & 0xFFF;

          if (flags & PAGE_USER) {
            uint64_t phys = pte & ~0xFFFULL;

            pmm::free_page(phys);

            pt[ti] = 0;
          }
        }
      }
    }
  }

  // NOTE: this only reclaims leaf (4K, PAGE_USER) pages. The intermediate
  // PT/PD/PDPT pages allocated for the process's private regions (e.g.
  // PML4 slot 1, and PML4[0]'s PDPT slot 1 -- see create_address_space())
  // are themselves never freed here, so those stay leaked on every
  // process exit. Reclaiming them safely requires knowing exactly which
  // intermediate tables are private vs shared with the kernel table
  // (only some are -- see the comment in create_address_space()), which
  // needs a look at address_space.cpp/elf.cpp to get right without
  // risking a double-free of a table the kernel or another process still
  // points at. Flagging this as a follow-up rather than guessing here.
}

bool map_page(uint64_t *pml4, uint64_t va, uint64_t pa, uint64_t flags) {
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
    if (!pdpt)
      return false;
    pml4[pml4_i] = (uint64_t)pdpt | upper_flags;
  } else {
    pdpt = (uint64_t *)(pml4[pml4_i] & ~0xFFF);
    // Only escalate flags (e.g. a kernel-only intermediate table that
    // now needs a PAGE_USER leaf under it); never silently downgrade an
    // entry another mapping still relies on.
    pml4[pml4_i] |= upper_flags;
  }

  // PDPT
  if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
    pd = (uint64_t *)alloc_page();
    if (!pd)
      return false;
    pdpt[pdpt_i] = (uint64_t)pd | upper_flags;
  } else {
    pd = (uint64_t *)(pdpt[pdpt_i] & ~0xFFF);
    pdpt[pdpt_i] |= upper_flags;
  }

  // PD
  if (!(pd[pd_i] & PAGE_PRESENT)) {
    pt = (uint64_t *)alloc_page();
    if (!pt)
      return false;
    pd[pd_i] = (uint64_t)pt | upper_flags;
  } else {
    pt = (uint64_t *)(pd[pd_i] & ~0xFFF);
    pd[pd_i] |= upper_flags;
  }

  // PTE
  bool was_present = pt[pt_i] & PAGE_PRESENT;

  pt[pt_i] = (pa & ~0xFFF) | flags;

  // Only needed when overwriting an existing translation (fresh PTEs
  // can't be stale in any TLB). Skipping this in the common
  // first-time-mapping case avoids an invlpg per page during process
  // creation / fork, which is the hot path here.
  if (was_present)
    asm volatile("invlpg (%0)" : : "r"(va) : "memory");

  return true;
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
  if (!pml4)
    return nullptr;

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
  if (!pdpt) {
    pmm::free_page((uintptr_t)pml4);
    return nullptr;
  }

  uint64_t *kernel_pdpt = (uint64_t *)(kernel_pml4[0] & ~0xFFFULL);

  if (kernel_pdpt) {
    for (int i = 0; i < 512; i++)
      pdpt[i] = kernel_pdpt[i];
  }

  pml4[0] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE;

  return pml4;
}

bool clone_user_pages(uint64_t *dst_pml4, uint64_t *src_pml4) {
  for (int pml4i = 0; pml4i < 256; pml4i++) {
    uint64_t pml4e = src_pml4[pml4i];

    if (!(pml4e & PAGE_PRESENT))
      continue;

    uint64_t *src_pdpt = (uint64_t *)(pml4e & ~0xFFFULL);

    for (int pi = 0; pi < 512; pi++) {
      uint64_t pdpte = src_pdpt[pi];

      if (!(pdpte & PAGE_PRESENT))
        continue;

      uint64_t *src_pd = (uint64_t *)(pdpte & ~0xFFFULL);

      for (int di = 0; di < 512; di++) {
        uint64_t pde = src_pd[di];

        if (!(pde & PAGE_PRESENT))
          continue;

        uint64_t *src_pt = (uint64_t *)(pde & ~0xFFFULL);

        for (int ti = 0; ti < 512; ti++) {
          uint64_t pte = src_pt[ti];

          if (!(pte & PAGE_PRESENT))
            continue;

          uint64_t flags = pte & 0xFFF;

          if (!(flags & PAGE_USER))
            continue;

          uint64_t va = ((uint64_t)pml4i << 39) | ((uint64_t)pi << 30) | ((uint64_t)di << 21) |
                        ((uint64_t)ti << 12);

          uint64_t phys = pte & ~0xFFFULL;

          void *page = alloc_page();

          if (!page)
            return false;

          memcpy(page, (void *)phys, 4096);

          if (!map_page(dst_pml4, va, (uint64_t)page, flags)) {
            pmm::free_page((uintptr_t)page);
            return false;
          }
        }
      }
    }
  }

  return true;
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
