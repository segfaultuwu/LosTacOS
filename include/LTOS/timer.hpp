#pragma once

#include <stdint.h>

namespace timer {

void init(uint32_t freq);

uint64_t ticks();

void sleep(uint64_t ms);
void tick();

uint64_t get_uptime_sec();
uint64_t get_uptime_ms();

} // namespace timer
