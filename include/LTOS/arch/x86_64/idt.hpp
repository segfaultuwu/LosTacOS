#pragma once

#include <stdint.h>

extern "C" {
void lidt(void *);
void isr0();
void irq0();
extern uint64_t isr_stub_table[256];
}

namespace idt {

void init();

}
