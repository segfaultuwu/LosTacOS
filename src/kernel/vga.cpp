#include "LTOS/vga.hpp"

namespace vga {

static volatile uint16_t *buffer = (uint16_t *)0xB8000;

static uint8_t color = 0x0F;

static int row = 0;
static int col = 0;

void clear() {
  for (int i = 0; i < 80 * 25; i++) {
    buffer[i] = (color << 8) | ' ';
  }

  row = 0;
  col = 0;
}

void put(char c) {
  if (c == '\n') {
    row++;
    col = 0;
    return;
  }

  buffer[row * 80 + col] = (color << 8) | c;

  col++;

  if (col >= 80) {
    col = 0;
    row++;
  }
}

void write(const char *str) {
  while (*str) {
    put(*str++);
  }
}

} // namespace vga
