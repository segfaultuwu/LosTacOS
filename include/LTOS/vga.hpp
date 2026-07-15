#pragma once

#include <stddef.h>
#include <stdint.h>

namespace vga {

void clear();
void put(char c);
void write(const char *str);

void set_color(uint8_t fg, uint8_t bg);

} // namespace vga
