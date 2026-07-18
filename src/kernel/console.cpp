#include "LTOS/console.hpp"

#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/psf.hpp"
#include "LTOS/drivers/serial.hpp"
#include <cstdint>
#include <string.h>

namespace console {

static psf::Font *font = nullptr;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t fg = 0xffffffff;
static uint32_t bg = 0x00000000;

static uint32_t screen_width;
static uint32_t screen_height;

static uint32_t scale = 1;

void clear();

void init() {
  screen_width = framebuffer::get_width();
  screen_height = framebuffer::get_height();

  framebuffer::clear(0x00202020);

  framebuffer::put_pixel(10, 10, 0x00ffffff);

  font = psf::get();

  if (!font) {
    framebuffer::put_pixel(20, 20, 0x00ff0000);
    return;
  }

  cursor_x = 0;
  cursor_y = 0;
  drivers::serial::writef("CONSOLE FONT=%lx\n", (uint64_t)font);
}

static void newline() {
  cursor_x = 0;
  cursor_y += font->height * scale;

  if (cursor_y + font->height >= screen_height) {
    // TODO: scroll
    cursor_y = 0;
  }
}

static void set_color(uint32_t new_fg, uint32_t new_bg) {
  fg = new_fg;
  bg = new_bg;
}

static void handle_ansi(const char *seq) {
  if (strcmp(seq, "[0m") == 0) {
    set_color(0xffffffff, 0x00000000);
  }

  else if (strcmp(seq, "[31m") == 0) {
    set_color(0xffff5555, bg);
  }

  else if (strcmp(seq, "[32m") == 0) {
    set_color(0xff55ff55, bg);
  }

  else if (strcmp(seq, "[33m") == 0) {
    set_color(0xffffff55, bg);
  }

  else if (strcmp(seq, "[34m") == 0) {
    set_color(0xff5555ff, bg);
  }

  else if (strcmp(seq, "[35m") == 0) {
    set_color(0xffff55ff, bg);
  }

  else if (strcmp(seq, "[36m") == 0) {
    set_color(0xff55ffff, bg);
  }

  else if (strcmp(seq, "[37m") == 0) {
    set_color(0xffffffff, bg);
  }

  else if (strcmp(seq, "[2J") == 0) {
    clear();
  }

  else if (strcmp(seq, "[H") == 0) {
    cursor_x = 0;
    cursor_y = 0;
  }
}

void put(char c) {
  if (!font)
    return;

  static bool escape = false;
  static char ansi[16];
  static size_t ansi_pos = 0;

  if (c == '\033') {
    escape = true;
    ansi_pos = 0;
    return;
  }

  if (escape) {
    if (ansi_pos < sizeof(ansi) - 1)
      ansi[ansi_pos++] = c;

    if (c == 'm' || c == 'J' || c == 'H' || c == 'K') {
      ansi[ansi_pos] = '\0';

      handle_ansi(ansi);

      escape = false;
      ansi_pos = 0;
    }

    return;
  }

  if (c == '\n') {
    newline();
    return;
  }

  uint8_t *glyph = psf::get_glyph(font, (uint8_t)c);

  if (!glyph) {
    framebuffer::put_pixel(30, 30, 0xffff0000);
    return;
  }

  if (!glyph)
    return;

  for (uint32_t row = 0; row < font->height; row++) {

    uint8_t *row_data = glyph + row * font->bytes_per_row;

    for (uint32_t byte = 0; byte < font->bytes_per_row; byte++) {

      uint8_t bits = row_data[byte];

      for (uint32_t bit = 0; bit < 8; bit++) {

        if (bits & (0x80 >> bit)) {

          for (uint32_t sy = 0; sy < scale; sy++) {
            for (uint32_t sx = 0; sx < scale; sx++) {

              framebuffer::put_pixel(cursor_x + (byte * 8 + bit) * scale + sx,
                                     cursor_y + row * scale + sy, fg);
            }
          }
        }
      }
    }
  }

  cursor_x += font->width * scale;

  if (cursor_x + font->width >= screen_width)
    newline();
}

void write(const char *buf, size_t len) {
  for (size_t i = 0; i < len; i++)
    put(buf[i]);
}

void clear() {
  framebuffer::clear(bg);

  cursor_x = 0;
  cursor_y = 0;
}

void set_font(psf::Font *f) { font = f; }

} // namespace console
