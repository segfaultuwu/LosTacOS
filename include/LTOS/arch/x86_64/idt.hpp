#pragma once

#include <stdint.h>

extern "C" {
void lidt(void *);
void isr0();
void irq0();
extern uint64_t isr_stub_table[256];
void irq1_handler();
}

namespace idt {

void init();
void set_gate(uint8_t vector, uint64_t handler);

} // namespace idt
