#include "LTOS/mm/heap.hpp"

namespace heap {

static uintptr_t current = HEAP_START;

void init() { current = HEAP_START; }

void *alloc(size_t size) {
  if (current + size >= HEAP_START + HEAP_SIZE)
    return nullptr;

  void *ptr = (void *)current;

  current += size;

  return ptr;
}

// TODO: implement free()
void free(void *) {
  // nothing for now lol
}

} // namespace heap
