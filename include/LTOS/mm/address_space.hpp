#pragma once

#include "LTOS/arch/x86_64/paging.hpp"

namespace mm {

class AddressSpace {
public:
  AddressSpace();

  void activate();

  void destroy();

  paging::PageTable *table;

  static AddressSpace *kernel();
  static AddressSpace *create();
};

} // namespace mm
