#include "LTOS/logger.hpp"

extern "C" void divide_error() {
  logger::error("Divide by zero");

  while (true) {
    asm volatile("hlt");
  }
}
