#pragma once

#include <stddef.h>
#include <stdint.h>
namespace vmm {
struct VMRegion {
  uintptr_t start;
  size_t size;
  uint64_t flags;
  VMRegion *next;
};

constexpr uint64_t PAGE_SIZE = 4096;

enum {
  VM_PRESENT = 1 << 0,
  VM_WRITE = 1 << 1,
  VM_USER = 1 << 2,
};

void init(uint64_t *pml4);

void map(uintptr_t virt, uintptr_t phys, uint64_t flags);

void unmap(uintptr_t virt);

uintptr_t alloc(size_t size, uint64_t flags);

} // namespace vmm
