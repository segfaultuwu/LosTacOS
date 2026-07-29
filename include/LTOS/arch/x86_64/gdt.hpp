#pragma once
#include <stdint.h>

namespace gdt {

void init();

void set_kernel_stack(uint64_t rsp0);

} // namespace gdt
