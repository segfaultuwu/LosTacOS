#include "LTOS/mm/heap.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/logger.hpp"

namespace heap {

static Block *heap_head = nullptr;
static Block *alloc_cursor = nullptr;

static size_t align(size_t size) {
  return (size + 15) & ~15;
}

static size_t *footer(Block *b) {
  return (size_t *)((uint8_t *)b + sizeof(Block) + b->size - sizeof(size_t));
}

static Block *prev_block(Block *b) {
  if ((uintptr_t)b <= HEAP_START)
    return nullptr;
  size_t *pf = (size_t *)((uint8_t *)b - sizeof(size_t));
  return (Block *)((uint8_t *)b - sizeof(Block) - *pf);
}

static Block *next_block(Block *b) {
  uintptr_t end = (uintptr_t)b + sizeof(Block) + b->size;
  if (end >= HEAP_START + HEAP_SIZE)
    return nullptr;
  return (Block *)end;
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
  *footer(heap_head) = heap_head->size;

  alloc_cursor = heap_head;

  heap_initialized = true;

  drivers::serial::writef("HEAP INIT addr=%lx size=%d\n", (uint64_t)heap_head, heap::HEAP_SIZE);
}

void *kmalloc(size_t size) {
  size = align(size);

  Block *curr = alloc_cursor ? alloc_cursor : heap_head;
  Block *start = curr;

  do {
    if (curr->free && curr->size >= size) {
      if (curr->size >= size + sizeof(Block) + 16) {
        Block *new_block = (Block *)((uint8_t *)curr + sizeof(Block) + size);

        new_block->size = curr->size - size - sizeof(Block);
        new_block->free = true;
        new_block->next = curr->next;
        *footer(new_block) = new_block->size;

        curr->next = new_block;
        curr->size = size;
        *footer(curr) = curr->size;
      }

      curr->free = false;
      alloc_cursor = curr;

      return (uint8_t *)curr + sizeof(Block);
    }

    curr = curr->next;
    if (!curr)
      curr = heap_head;
  } while (curr != start);

  return nullptr;
}

void kfree(void *ptr) {
  if (!ptr)
    return;

  uintptr_t p = (uintptr_t)ptr;
  if (p < HEAP_START + sizeof(Block) || p >= HEAP_START + HEAP_SIZE)
    return;

  Block *block = (Block *)((uint8_t *)ptr - sizeof(Block));

  if (block->free)
    return;

  block->free = true;

  if (alloc_cursor && (uintptr_t)block < (uintptr_t)alloc_cursor)
    alloc_cursor = block;

  Block *next = next_block(block);
  if (next && next->free && (uintptr_t)next < HEAP_START + HEAP_SIZE) {
    if (next->next) {
      Block *tmp = next->next;
      while (tmp) {
        if (tmp == block)
          break;
        tmp = tmp->next;
      }
    }
    block->size += sizeof(Block) + next->size;
    block->next = next->next;
    *footer(block) = block->size;

    if (alloc_cursor == next)
      alloc_cursor = block;
  }

  Block *prev = prev_block(block);
  if (prev && prev->free && prev != block) {
    if (prev->next != block) {
      Block *tmp = heap_head;
      while (tmp) {
        if (tmp->next == block) {
          prev = tmp;
          break;
        }
        tmp = tmp->next;
      }
    }

    if (prev->free && prev->next == block) {
      prev->size += sizeof(Block) + block->size;
      prev->next = block->next;
      *footer(prev) = prev->size;

      if (alloc_cursor == block)
        alloc_cursor = prev;
    }
  }
}

} // namespace heap
