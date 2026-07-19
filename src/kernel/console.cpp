#include "LTOS/drivers/console.hpp"

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

  font = psf::get();

  framebuffer::clear(0x00000000);

  if (!font) {
    framebuffer::put_pixel(10, 10, 0xffff0000);
    return;
  }

  cursor_x = 0;
  cursor_y = 0;

  drivers::serial::writef("CONSOLE FONT=%lx\n", (uint64_t)font);
}

void draw_char(char c) {
  uint8_t *glyph = psf::get_glyph(font, (uint8_t)c);

  if (!glyph)
    return;

  for (uint32_t row = 0; row < font->height; row++) {
    for (uint32_t byte = 0; byte < font->bytes_per_row; byte++) {
      uint8_t bits = glyph[row * font->bytes_per_row + byte];

      for (uint32_t bit = 0; bit < 8; bit++) {
        uint32_t x = byte * 8 + bit;

        if (x >= font->width)
          continue;

        if (bits & (0x80 >> bit)) {
          framebuffer::put_pixel(cursor_x + x, cursor_y + row, fg);
        }
      }
    }
  }

  cursor_x += font->width;

  if (cursor_x + font->width >= screen_width)
    newline();
}

void scroll() {
  uint32_t pitch = framebuffer::get_pitch();

  uint8_t *fb = framebuffer::get_backbuffer();

  if (!fb)
    return;

  uint32_t move = pitch * (screen_height - font->height);

  memcpy(fb, fb + pitch * font->height, move);

  memset(fb + move, 0, pitch * font->height);
}

void newline() {
  cursor_x = 0;
  cursor_y += font->height * scale;

  if (cursor_y + font->height * scale > screen_height) {
    scroll();

    cursor_y -= font->height * scale;
  }
}

static void set_color(uint32_t new_fg, uint32_t new_bg) {

  fg = new_fg;
  bg = new_bg;
}

static void handle_ansi(const char *seq) {

  if (strcmp(seq, "[0m") == 0)
    set_color(0xffffffff, bg);

  else if (strcmp(seq, "[31m") == 0)
    set_color(0xffff5555, bg);

  else if (strcmp(seq, "[32m") == 0)
    set_color(0xff55ff55, bg);

  else if (strcmp(seq, "[33m") == 0)
    set_color(0xffffff55, bg);

  else if (strcmp(seq, "[34m") == 0)
    set_color(0xff5555ff, bg);

  else if (strcmp(seq, "[35m") == 0)
    set_color(0xffff55ff, bg);

  else if (strcmp(seq, "[36m") == 0)
    set_color(0xff55ffff, bg);

  else if (strcmp(seq, "[37m") == 0)
    set_color(0xffffffff, bg);

  else if (strcmp(seq, "[2J") == 0)
    clear();

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

  if (c == '\t') {
    cursor_x += 2;
  }

  if (escape) {

    if (ansi_pos < sizeof(ansi) - 1)
      ansi[ansi_pos++] = c;

    if (c == 'm' || c == 'J' || c == 'H') {

      ansi[ansi_pos] = 0;

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

  if (c == '\r') {
    cursor_x = 0;
    return;
  }

  if (c == 0x08) {
    backspace();
    return;
  }

  if (c == '\t') {
    cursor_x += 4 * font->width * scale;
    return;
  }

  if (cursor_x + font->width * scale >= screen_width)
    newline();

  draw_char(c);
}

void backspace() {
  if (cursor_x < font->width)
    return;

  cursor_x -= font->width;

  for (uint32_t y = 0; y < font->height; y++) {
    for (uint32_t x = 0; x < font->width; x++) {
      framebuffer::put_pixel(cursor_x + x, cursor_y + y, bg);
    }
  }

  framebuffer::swap();
}

void put_swap(char c) {
  put(c);
  framebuffer::swap();
}

void write(const char *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    put(buf[i]);
  }
  framebuffer::swap();
}

void clear() {

  framebuffer::clear(bg);

  cursor_x = 0;
  cursor_y = 0;
}

void set_font(psf::Font *f) {
  font = f;
}

} // namespace console
