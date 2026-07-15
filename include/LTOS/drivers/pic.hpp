#pragma once

#include <stdint.h>

namespace drivers::pic {
void init();
void eoi(uint8_t irq);
} // namespace drivers::pic
