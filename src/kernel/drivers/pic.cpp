#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"

namespace drivers::pic {

#define PIC1 0x20
#define PIC2 0xA0

#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)

#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

void io_wait() {
  drivers::serial::outb(0x80, 0);
}

void init() {
  // ICW1
  drivers::serial::outb(PIC1_COMMAND, 0x11);
  io_wait();

  drivers::serial::outb(PIC2_COMMAND, 0x11);
  io_wait();

  // ICW2 - vector offset
  drivers::serial::outb(PIC1_DATA, 0x20); // IRQ0-7 -> 32-39
  io_wait();

  drivers::serial::outb(PIC2_DATA, 0x28); // IRQ8-15 -> 40-47
  io_wait();

  // ICW3
  drivers::serial::outb(PIC1_DATA, 4);
  io_wait();

  drivers::serial::outb(PIC2_DATA, 2);
  io_wait();

  // ICW4
  drivers::serial::outb(PIC1_DATA, 0x01);
  io_wait();

  drivers::serial::outb(PIC2_DATA, 0x01);
  io_wait();

  // enable IRQ0 only
  drivers::serial::outb(PIC1_DATA, 0xFE);

  // disable slave
  drivers::serial::outb(PIC2_DATA, 0xFF);
}

void enable_irq(uint8_t irq) {
  uint16_t port;

  if (irq < 8)
    port = 0x21;
  else {
    port = 0xA1;
    irq -= 8;
  }

  uint8_t mask = drivers::serial::inb(port);

  mask &= ~(1 << irq);

  drivers::serial::outb(port, mask);
}

void eoi(uint8_t irq) {
  if (irq >= 8)
    drivers::serial::outb(PIC2_COMMAND, 0x20);

  drivers::serial::outb(PIC1_COMMAND, 0x20);
}

} // namespace drivers::pic
