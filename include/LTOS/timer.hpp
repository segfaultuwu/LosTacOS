#pragma once

#include <stdint.h>

namespace timer {

void init(uint32_t frequency);

uint64_t ticks();

void sleep(uint64_t ms);

} // namespace timer
