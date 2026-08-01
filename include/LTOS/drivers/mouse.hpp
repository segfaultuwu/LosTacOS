#pragma once

#include "LTOS/sched/task.hpp"
#include <stdint.h>
#include <stdbool.h>

namespace drivers::mouse {

struct MouseState {
  int x;
  int y;
  bool left;
  bool right;
  bool middle;
};

void init();
void register_dev();
void irq_handler();
MouseState get_state();

} // namespace drivers::mouse

extern "C" sched::Registers *mouse_irq(sched::Registers *regs);
