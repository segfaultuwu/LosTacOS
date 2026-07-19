#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/timer.hpp"

extern "C" void timer_irq() {
  timer::tick();

  drivers::pic::eoi(0);
}

extern "C" void keyboard_irq() {
  drivers::keyboard::irq_handler();
}
