#pragma once
#include <stdint.h>

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER (1 << 2)

namespace paging {

void init();
void enable_paging();

void reserve_below(uint64_t addr);

void setup_kernel_identity();

uint64_t *create_address_space();
void map_page(uint64_t *pml4, uint64_t va, uint64_t pa, uint64_t flags);

void *alloc_page();
} // namespace paging
