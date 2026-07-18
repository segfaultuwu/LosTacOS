#include "LTOS/console.hpp"

#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/psf.hpp"

namespace console {

static psf::Font *font = nullptr;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t fg = 0xffffffff;
static uint32_t bg = 0x00000000;

static uint32_t screen_width;
static uint32_t screen_height;

void init() {
  screen_width = framebuffer::get_width();
  screen_height = framebuffer::get_height();

  cursor_x = 0;
  cursor_y = 0;

  font = psf::get();

  framebuffer::clear(bg);
}

static void newline() {
  cursor_x = 0;
  cursor_y += font->height;

  if (cursor_y + font->height >= screen_height) {
    // TODO: scroll
    cursor_y = 0;
  }
}

void put(char c) {
  if (!font)
    return;

  if (c == '\n') {
    newline();
    return;
  }

  uint8_t *glyph = psf::get_glyph(font, (uint8_t)c);

  for (uint32_t row = 0; row < font->height; row++) {
    uint8_t bits = glyph[row];

    for (uint32_t col = 0; col < font->width; col++) {
      if (bits & (0x80 >> col)) {
        framebuffer::put_pixel(cursor_x + col, cursor_y + row, fg);
      }
    }
  }

  cursor_x += font->width;

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
