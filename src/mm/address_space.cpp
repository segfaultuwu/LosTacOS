#include "LTOS/mm/address_space.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include <new>

namespace mm {

static AddressSpace kernel_space;

AddressSpace::AddressSpace() {
  table = nullptr;
}

AddressSpace *AddressSpace::kernel() {
  return &kernel_space;
}

AddressSpace *AddressSpace::create() {
  AddressSpace *space = (AddressSpace *)heap::kmalloc(sizeof(AddressSpace));

  if (!space)
    return nullptr;

  new (space) AddressSpace();

  space->table = paging::clone_kernel_table();

  return space;
}

void AddressSpace::destroy() {
  if (table) {
    heap::kfree(table);
    table = nullptr;
  }

  heap::kfree(this);
}

void AddressSpace::activate() {
  if (!table)
    return;

  paging::switch_page_table(table);
}

} // namespace mm
