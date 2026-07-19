#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/timer.hpp"

extern "C" void timer_irq() {
  timer::tick();

  drivers::pic::eoi(0);
}
