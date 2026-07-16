#pragma once

#include <stddef.h>
#include <stdint.h>

namespace heap {

// For future use (Free List Allocator)
struct Block {
  size_t size;
  bool free;
  Block *next;
};

void init();
void *alloc(size_t size);
void free(void *ptr);

constexpr uintptr_t HEAP_START = 0x1000000; // 16MB
constexpr size_t HEAP_SIZE = 0x100000;      // 2MB

} // namespace heap
