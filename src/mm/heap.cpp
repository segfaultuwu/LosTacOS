#include "LTOS/mm/heap.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/logger.hpp"

namespace heap {

static Block *heap_head = nullptr;

static size_t align(size_t size) {
  return (size + 15) & ~15;
}

bool heap_initialized = false;

void init() {
  if (heap_initialized) {
    logger::warn("Heap already initialized");
    return;
  }

  heap_head = (Block *)HEAP_START;

  heap_head->size = HEAP_SIZE - sizeof(Block);
  heap_head->free = true;
  heap_head->next = nullptr;

  heap_initialized = true;

  drivers::serial::writef("HEAP INIT addr=%lx size=%d\n", (uint64_t)heap_head, heap::HEAP_SIZE);
}

// deprecated.
// void *alloc(size_t size) {
//   if (current + size >= HEAP_START + HEAP_SIZE)
//     return nullptr;

//   void *ptr = (void *)current;

//   current += size;

//   return ptr;
// }

// Should be good enough..?
void *kmalloc(size_t size) {
  Block *curr = heap_head;

  // drivers::serial::writef("kmalloc request=%u\n", size);

  size = align(size);

  while (curr) {
    if (curr->free && curr->size >= size) {
      // split if there is enough space
      if (curr->size >= size + sizeof(Block) + 8) {
        Block *new_block = (Block *)((uint8_t *)curr + sizeof(Block) + size);

        new_block->size = curr->size - size - sizeof(Block);

        new_block->free = true;
        new_block->next = curr->next;

        curr->next = new_block;
        curr->size = size;
      }

      curr->free = false;

      return (uint8_t *)curr + sizeof(Block);
    }

    curr = curr->next;
  }

  return nullptr; // Oout of memory
}

void kfree(void *ptr) {
  if (!ptr)
    return;

  Block *block = (Block *)((uint8_t *)ptr - sizeof(Block));

  block->free = true;

  // merge
  Block *curr = heap_head;

  while (curr && curr->next) {
    if (curr->free && curr->next->free) {
      curr->size += sizeof(Block) + curr->next->size;

      curr->next = curr->next->next;
    } else {
      curr = curr->next;
    }
  }
}

} // namespace heap
