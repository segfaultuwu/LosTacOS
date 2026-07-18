#pragma once
#include <stdint.h>

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER (1 << 2)

namespace paging {

// Defined once in paging.cpp. This must NOT be 'static' here: a static
// array declared in a header gives every translation unit that includes
// this file its own private copy, silently disconnected from the one
// actually loaded into CR3 -- any other .cpp writing to
// "paging::kernel_pml4" would be modifying dead memory the CPU never
// looks at.
extern uint64_t kernel_pml4[512];

void init();
void enable_paging();

void reserve_below(uint64_t addr);

void setup_kernel_identity();

uint64_t *create_address_space();
void map_page(uint64_t *pml4, uint64_t va, uint64_t pa, uint64_t flags);
void unmap_page(uint64_t *pml4, uint64_t va);

void *alloc_page();
} // namespace paging
