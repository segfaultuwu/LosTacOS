#pragma once

#include <cstddef>
#include <stdint.h>

namespace pmm {

struct MemoryRegion {
  uint64_t base;
  uint64_t length;
  uint32_t type;
};

static MemoryRegion regions[128];
static size_t region_count = 0;

void init(uint64_t multiboot_addr);

uintptr_t alloc_page();

void free_page(uintptr_t addr);

uint64_t free_memory();

void print_memory_map();

} // namespace pmm
