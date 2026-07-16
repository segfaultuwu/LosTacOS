#include "LTOS/lib/kprintf.h"
#include "LTOS/panic.hpp"
#include <cstdint>

extern "C" void divide_error() { panic::halt("Divide by zero"); }

extern "C" void unhandled_interrupt(uint64_t vector) {
  kprintf("Unhandled interrupt/exception: vector=%d\n", vector);
  panic::halt("Unhandled interrupt/exception");
}
