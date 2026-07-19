#pragma once

#include <stdint.h>

namespace drivers::pic {
void init();
void enable_irq(uint8_t irq);
void eoi(uint8_t irq);
} // namespace drivers::pic
