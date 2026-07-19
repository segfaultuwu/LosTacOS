#include "LTOS/mm/heap.hpp"
#include <stddef.h>

extern "C" {

void *malloc(size_t size) {
  return heap::kmalloc(size);
}

void free(void *ptr) {
  return heap::kfree(ptr);
}
}
