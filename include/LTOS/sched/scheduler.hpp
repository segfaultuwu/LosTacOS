#pragma once

#include "task.hpp"

namespace sched {

void init();

void create(void (*entry)());

Registers *schedule(Registers *current);

Task *get_current();

} // namespace sched
