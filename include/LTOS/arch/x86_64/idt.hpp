#pragma once

#include <stdint.h>

extern "C" {
void lidt(void *);

void isr0();
}

namespace idt {

void init();

}
