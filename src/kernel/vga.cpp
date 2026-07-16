#include "LTOS/vga.hpp"
#include "LTOS/drivers/serial.hpp"
#include "string.h"

namespace vga {

constexpr size_t VGA_WIDTH = 80;
constexpr size_t VGA_HEIGHT = 25;

static uint16_t *buffer = (uint16_t *)0xB8000;

size_t row = 0;
size_t col = 0;

uint8_t color = 0x07;

void set_color(uint8_t fg, uint8_t bg) { color = (bg << 4) | (fg & 0x0F); }

void clear() {
  for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    buffer[i] = ((uint16_t)color << 8) | ' ';
  }

  row = 0;
  col = 0;
}

void clear_line() {
  for (size_t x = 0; x < VGA_WIDTH; x++) {
    buffer[row * VGA_WIDTH + x] = ((uint16_t)color << 8) | ' ';
  }

  col = 0;
}

void home() {
  row = 0;
  col = 0;
}

void scroll() {
  for (size_t y = 1; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      buffer[(y - 1) * VGA_WIDTH + x] = buffer[y * VGA_WIDTH + x];
    }
  }

  for (size_t x = 0; x < VGA_WIDTH; x++) {
    buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ((uint16_t)color << 8) | ' ';
  }

  row = VGA_HEIGHT - 1;
  col = 0;
}

void newline() {
  col = 0;
  row++;

  if (row >= VGA_HEIGHT)
    scroll();
}

void put_raw(char c) {
  buffer[row * VGA_WIDTH + col] = ((uint16_t)color << 8) | c;

  col++;

  if (col >= VGA_WIDTH)
    newline();
}

void handle_ansi(const char *seq) {
  if (strcmp(seq, "[0m") == 0) {
    set_color(7, 0);
  }

  else if (strcmp(seq, "[31m") == 0) {
    set_color(4, 0);
  }

  else if (strcmp(seq, "[32m") == 0) {
    set_color(2, 0);
  }

  else if (strcmp(seq, "[33m") == 0) {
    set_color(6, 0);
  }

  else if (strcmp(seq, "[34m") == 0) {
    set_color(1, 0);
  }

  else if (strcmp(seq, "[35m") == 0) {
    set_color(5, 0);
  }

  else if (strcmp(seq, "[36m") == 0) {
    set_color(3, 0);
  }

  else if (strcmp(seq, "[37m") == 0) {
    set_color(7, 0);
  }

  else if (strcmp(seq, "[2J") == 0) {
    clear();
  }

  else if (strcmp(seq, "[H") == 0) {
    home();
  }

  else if (strcmp(seq, "[K") == 0) {
    clear_line();
  }
}

void put(char c) {
  static bool escape = false;
  static char ansi[16];
  static size_t ansi_pos = 0;

  if (c == '\033') {
    escape = true;
    ansi_pos = 0;
    return;
  }

  if (escape) {
    ansi[ansi_pos++] = c;

    if (c == 'm' || c == 'J' || c == 'H' || c == 'K') {
      ansi[ansi_pos] = '\0';

      handle_ansi(ansi);

      escape = false;
      ansi_pos = 0;
    }

    return;
  }

  switch (c) {
  case '\n':
    newline();
    return;

  case '\r':
    col = 0;
    return;

  case '\t':
    for (int i = 0; i < 4; i++)
      put(' ');

    return;

  case '\b':
    if (col > 0) {
      col--;

      buffer[row * VGA_WIDTH + col] = ((uint16_t)color << 8) | ' ';
    }

    return;
  }

  put_raw(c);
}

void write(const char *str) {
  while (*str) {
    put(*str++);
  }
}

} // namespace vga
