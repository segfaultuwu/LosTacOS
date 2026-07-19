#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/drivers/pic.hpp"
#include <cstdint>

namespace idt {

struct IDTEntry {
  uint16_t offset_low;
  uint16_t selector;

  uint8_t ist;

  uint8_t type_attr;

  uint16_t offset_mid;

  uint32_t offset_high;

  uint32_t zero;
} __attribute__((packed));

struct IDTPointer {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

static IDTEntry idt[256];

void set_gate(uint8_t vector, uint64_t handler) {
  IDTEntry &entry = idt[vector];

  entry.offset_low = handler & 0xffff;

  entry.selector = 0x08;

  entry.ist = 0;

  entry.type_attr = 0x8E;

  entry.offset_mid = (handler >> 16) & 0xffff;

  entry.offset_high = (handler >> 32);

  entry.zero = 0;
}

void init() {
  IDTPointer idtr;

  for (int i = 0; i < 256; i++) {
    set_gate(i, isr_stub_table[i]); // was: set_gate(i, 0);
  }
  set_gate(0, (uint64_t)isr_stub_table[0]);
  set_gate(32, (uint64_t)irq0);
  set_gate(33, (uint64_t)irq1_handler);

  drivers::pic::enable_irq(1);

  idtr.limit = sizeof(idt) - 1;

  idtr.base = (uint64_t)&idt;

  lidt(&idtr);
}

} // namespace idt
