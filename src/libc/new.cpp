#include "LTOS/mm/heap.hpp"
#include <stddef.h>

void *operator new(size_t size) { return heap::alloc(size); }

void *operator new[](size_t size) { return heap::alloc(size); }

void operator delete(void *ptr) { heap::free(ptr); }

void operator delete[](void *ptr) { heap::free(ptr); }
