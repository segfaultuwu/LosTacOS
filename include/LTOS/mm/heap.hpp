#pragma once

#include <stddef.h>
#include <stdint.h>

namespace heap {

struct Block {
  size_t size;
  bool free;
  Block *next;
};

void init();
void *kmalloc(size_t size);
void kfree(void *ptr);

constexpr uintptr_t HEAP_START = 0x1000000; // 16MB
constexpr size_t HEAP_SIZE = 0x100000;      // 2MB

} // namespace heap
