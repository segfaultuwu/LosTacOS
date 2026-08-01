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

  if (!space->table) {
    heap::kfree(space);
    return nullptr;
  }

  return space;
}

void AddressSpace::destroy() {
  if (table && table != (paging::PageTable *)paging::kernel_pml4) {
    paging::destroy_user_pages(table->pml4);

    heap::kfree(table);
  }

  table = nullptr;

  heap::kfree(this);
}

void AddressSpace::activate() {
  if (!table)
    return;

  paging::switch_page_table(table);
}

} // namespace mm
