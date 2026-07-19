#include "LTOS/mm/pmm.hpp"
#include "LTOS/kernel.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"

#include <cstdint>
#include <multiboot.h>
#include <stddef.h>
#include <stdint.h>

namespace pmm {

constexpr uint64_t PAGE_SIZE = 4096;

static uint64_t last_page = 0;

// Max 128GB of RAM (who has more in the big 26? :wilted_rose:)
constexpr uint64_t MAX_MEMORY = 128ULL * 1024 * 1024 * 1024;

constexpr size_t TOTAL_PAGES = MAX_MEMORY / PAGE_SIZE;

// 128GB / 4096 / 8 = 128KB bitmap
static uint8_t bitmap[TOTAL_PAGES / 8];

static uint64_t total_pages = 0;
static uint64_t free_pages_count = 0;

static inline void set_bit(size_t bit) {
  bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void clear_bit(size_t bit) {
  bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool test_bit(size_t bit) {
  return bitmap[bit / 8] & (1 << (bit % 8));
}

static void reserve_region(uint64_t addr, uint64_t length) {
  uint64_t start = addr / PAGE_SIZE;
  uint64_t end = (addr + length + PAGE_SIZE - 1) / PAGE_SIZE;

  for (uint64_t i = start; i < end; i++) {
    if (i < TOTAL_PAGES) {
      if (!test_bit(i)) {
        set_bit(i);
        free_pages_count--;
      }
    }
  }
}

static void free_region(uint64_t addr, uint64_t length) {
  uint64_t start = addr / PAGE_SIZE;
  uint64_t end = (addr + length) / PAGE_SIZE;

  // kprintf("FREE REGION: %lx - %lx\n", addr, length);

  for (uint64_t i = start; i < end; i++) {
    if (i < TOTAL_PAGES) {
      if (test_bit(i)) {
        clear_bit(i);
        free_pages_count++;
      }
    }
  }
}

constexpr uint32_t TAG_END = 0;
constexpr uint32_t TAG_MMAP = 6;
constexpr uint32_t MEMORY_AVAILABLE = 1;

void init(uint64_t multiboot_addr) {
  for (size_t i = 0; i < sizeof(bitmap); i++)
    bitmap[i] = 0xff;

  free_pages_count = 0;
  total_pages = 0;

  auto tag = (multiboot_tag *)(uintptr_t)(multiboot_addr + 8);

  while (tag->type != TAG_END) {
    if (tag->type == TAG_MMAP) {
      auto mmap = (multiboot_tag_mmap *)tag;

      auto entry = (multiboot_mmap_entry *)((uint8_t *)mmap + 16);

      uint32_t count = (mmap->size - 16) / mmap->entry_size;

      for (uint32_t i = 0; i < count; i++) {

        if (region_count < MAX_REGIONS) {
          regions[region_count].base = entry->addr;
          regions[region_count].length = entry->len;
          regions[region_count].type = entry->type;
          region_count++;
        }

        if (entry->type == MEMORY_AVAILABLE) {

          total_pages += entry->len / PAGE_SIZE;

          free_region(entry->addr, entry->len);
        }

        entry = (multiboot_mmap_entry *)((uint8_t *)entry + mmap->entry_size);
      }
    }

    tag = (multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
  }

  reserve_region(0, 0x100000);
  reserve_region((uint64_t)&_kernel_start, (uint64_t)&_kernel_end - (uint64_t)&_kernel_start);
}

uintptr_t alloc_page() {
  uint64_t i;
  for (i = last_page; i < TOTAL_PAGES; i++) {

    if (!test_bit(i)) {

      set_bit(i);

      free_pages_count--;

      return i * PAGE_SIZE;
    }
  }

  return 0;
}

void free_page(uintptr_t addr) {

  uint64_t page = addr / PAGE_SIZE;

  if (test_bit(page)) {
    clear_bit(page);
    free_pages_count++;
  }
}

// Helpers

uint64_t free_memory() {
  return free_pages_count * PAGE_SIZE;
}

uint64_t total_memory() {
  return total_pages * PAGE_SIZE;
}

uint64_t used_memory() {
  uint64_t total = total_memory();
  uint64_t free = free_memory();

  if (free > total)
    return 0;

  return total - free;
}

static const char *type_name(uint32_t type) {
  switch (type) {
  case 1:
    return "AVAILABLE";

  case 3:
    return "ACPI";

  case 4:
    return "NVS";

  case 5:
    return "BAD";

  default:
    return "RESERVED";
  }
}

void print_memory_map() {
  kprintf("Memory Map\n");

  kprintf("+------------------+------------------+------------+\n");
  kprintf("| %-16s | %-16s | %-10s |\n", "BASE", "LENGTH", "TYPE");
  kprintf("+------------------+------------------+------------+\n");

  for (size_t i = 0; i < region_count; i++) {
    kprintf("| %016lx | %016lx | %-10s |\n", regions[i].base, regions[i].length,
            type_name(regions[i].type));
  }

  kprintf("+------------------+------------------+------------+\n");

  uint64_t used_mb = used_memory() / (1024 * 1024);
  uint64_t total_mb = total_memory() / (1024 * 1024);

  kprintf("RAM: %lu/%lu MB\n", used_mb, total_mb);

  kprintf("Free: %lu MB\n", free_memory() / 1024 / 1024);

  kprintf("total_pages=%lu\n", total_pages);
  kprintf("free_pages=%lu\n", free_pages_count);
}

} // namespace pmm
