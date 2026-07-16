#include "LTOS/mm/heap.hpp"
#include <stddef.h>

void *operator new(size_t size) { return heap::kmalloc(size); }

void *operator new[](size_t size) { return heap::kmalloc(size); }

void operator delete(void *ptr) { heap::kfree(ptr); }

void operator delete[](void *ptr) { heap::kfree(ptr); }
