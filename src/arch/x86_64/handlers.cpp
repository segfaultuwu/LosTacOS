#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/sched/scheduler.hpp"

extern "C" sched::Registers *timer_irq(sched::Registers *regs) {
  static uint64_t count = 0;

  if (++count % 100 == 0)
    logger::info("IRQ0");

  timer::tick();

  drivers::pic::eoi(0);

  return sched::schedule(regs);
}

extern "C" sched::Registers *keyboard_irq(sched::Registers *regs) {
  drivers::keyboard::irq_handler();

  drivers::pic::eoi(1);

  return regs;
}
