#pragma once

#include <stdint.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)

namespace paging {

struct PageTable {

  /*
      Virtual pointer to PML4
  */
  uint64_t *pml4;

  /*
      Physical address used by CR3
  */
  uint64_t phys;

  /*
      Create new address space
  */
  static PageTable *create();

  /*
      Free address space
  */
  void destroy();

  /*
      Map virtual -> physical
  */
  bool map(uint64_t virt, uint64_t phys, uint64_t flags);

  /*
      Remove mapping
  */
  bool unmap(uint64_t virt);

  /*
      Get physical address
  */
  uint64_t virt_to_phys(uint64_t virt);

  /*
      Load CR3
  */
  void activate();
};

// kernel address space
extern uint64_t kernel_pml4[512];

// init paging
void init();

void enable_paging();

// kernel identity mapping
void setup_kernel_identity();

// low memory reserve
void reserve_below(uint64_t addr);

// low level functions

uint64_t *create_address_space();

void map_page(uint64_t *pml4, uint64_t va, uint64_t pa, uint64_t flags);

void unmap_page(uint64_t *pml4, uint64_t va);

void *alloc_page();

} // namespace paging
