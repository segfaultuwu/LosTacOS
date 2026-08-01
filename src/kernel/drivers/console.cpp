#include "LTOS/drivers/console.hpp"

#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/psf.hpp"
#include "LTOS/drivers/serial.hpp"

#include <stdint.h>
#include <string.h>

namespace console {

static psf::Font *font = nullptr;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t fg = 0xffffffff;
static uint32_t bg = 0x00000000;

static bool bold = false;
static bool reverse = false;

static uint32_t screen_width;
static uint32_t screen_height;

static uint32_t scale = 1;

// --- blinking cursor state ---
static bool cursor_visible = true;
static uint32_t cursor_blink_counter = 0;
static const uint32_t CURSOR_BLINK_PERIOD = 120;

// --- dirty-rect tracking (avoids full-framebuffer swap per char) ---
static uint32_t dirty_x0 = UINT32_MAX;
static uint32_t dirty_y0 = UINT32_MAX;
static uint32_t dirty_x1 = 0;
static uint32_t dirty_y1 = 0;
static bool has_dirty = false;

void lock() {}

void unlock() {}

void clear();

static void mark_dirty(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
  if (w == 0 || h == 0)
    return;

  if (x < dirty_x0)
    dirty_x0 = x;
  if (y < dirty_y0)
    dirty_y0 = y;
  if (x + w > dirty_x1)
    dirty_x1 = x + w;
  if (y + h > dirty_y1)
    dirty_y1 = y + h;

  has_dirty = true;
}

static void flush_dirty() {
  if (!has_dirty)
    return;

  framebuffer::swap_rect(dirty_x0, dirty_y0, dirty_x1 - dirty_x0, dirty_y1 - dirty_y0);

  dirty_x0 = UINT32_MAX;
  dirty_y0 = UINT32_MAX;
  dirty_x1 = 0;
  dirty_y1 = 0;
  has_dirty = false;
}

static void clear_line(int mode) {
  uint32_t start = 0;
  uint32_t end = screen_width;

  if (mode == 0) {
    // cursor -> end
    start = cursor_x;
  } else if (mode == 1) {
    // start -> cursor
    end = cursor_x;
  } else if (mode == 2) {
    // whole line
    start = 0;
    end = screen_width;
  }

  for (uint32_t y = 0; y < font->height; y++) {
    for (uint32_t x = start; x < end; x++) {
      framebuffer::put_pixel(x, cursor_y + y, bg);
    }
  }

  mark_dirty(start, cursor_y, end - start, font->height);
}

static void draw_cursor_block(bool on) {
  (void)on;
  if (!font)
    return;

  uint32_t w = font->width * scale;
  uint32_t h = font->height * scale;

  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      uint32_t px = framebuffer::get_pixel(cursor_x + x, cursor_y + y);
      uint32_t inverted = px ^ 0x00FFFFFF;
      framebuffer::put_pixel(cursor_x + x, cursor_y + y, inverted);
    }
  }

  mark_dirty(cursor_x, cursor_y, w, h);
}

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

  cursor_visible = true;
  cursor_blink_counter = 0;

  // Initial clear is genuinely full-screen — flush it once, directly,
  // rather than routing through the dirty-rect machinery.
  framebuffer::swap();

  drivers::serial::writef("CONSOLE FONT=%lx\n", (uint64_t)font);
}

void draw_char(char c) {
  uint8_t *glyph = psf::get_glyph(font, (uint8_t)c);

  if (!glyph)
    return;

  uint32_t fg_color = reverse ? bg : fg;
  uint32_t bg_color = reverse ? fg : bg;

  uint32_t start_x = cursor_x;
  uint32_t start_y = cursor_y;

  for (uint32_t row = 0; row < font->height; row++) {
    for (uint32_t byte = 0; byte < font->bytes_per_row; byte++) {
      uint8_t bits = glyph[row * font->bytes_per_row + byte];

      for (uint32_t bit = 0; bit < 8; bit++) {
        uint32_t x = byte * 8 + bit;

        if (x >= font->width)
          continue;

        uint32_t color = (bits & (0x80 >> bit)) ? fg_color : bg_color;
        framebuffer::put_pixel(cursor_x + x, cursor_y + row, color);
      }
    }
  }

  mark_dirty(start_x, start_y, font->width, font->height);

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

  memmove(fb, fb + pitch * font->height, move);

  memset(fb + move, 0, pitch * font->height);

  // A scroll genuinely touches the entire visible area.
  mark_dirty(0, 0, screen_width, screen_height);
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

static void ansi_code(int code) {
  switch (code) {

  case 0:
    fg = 0xffffffff;
    bg = 0x00000000;
    bold = false;
    reverse = false;
    break;

  case 1:
    bold = true;
    break;

  case 2:
    fg &= 0xff777777;
    break;

  case 7:
    reverse = true;
    break;

  case 22:
    bold = false;
    break;

  case 27:
    reverse = false;
    break;

    // foreground

  case 30:
    fg = 0xff000000;
    break;
  case 31:
    fg = 0xffff5555;
    break;
  case 32:
    fg = 0xff55ff55;
    break;
  case 33:
    fg = 0xffffff55;
    break;
  case 34:
    fg = 0xff5555ff;
    break;
  case 35:
    fg = 0xffff55ff;
    break;
  case 36:
    fg = 0xff55ffff;
    break;
  case 37:
    fg = 0xffffffff;
    break;

    // bright foreground

  case 90:
    fg = 0xff555555;
    break;
  case 91:
    fg = 0xffff5555;
    break;
  case 92:
    fg = 0xff55ff55;
    break;
  case 93:
    fg = 0xffffff55;
    break;
  case 94:
    fg = 0xff5555ff;
    break;
  case 95:
    fg = 0xffff55ff;
    break;
  case 96:
    fg = 0xff55ffff;
    break;
  case 97:
    fg = 0xffffffff;
    break;

    // background

  case 40:
    bg = 0xff000000;
    break;
  case 41:
    bg = 0xffff5555;
    break;
  case 42:
    bg = 0xff55ff55;
    break;
  case 43:
    bg = 0xffffff55;
    break;
  case 44:
    bg = 0xff5555ff;
    break;
  case 45:
    bg = 0xffff55ff;
    break;
  case 46:
    bg = 0xff55ffff;
    break;
  case 47:
    bg = 0xffffffff;
    break;

  case 100:
    bg = 0xff555555;
    break;
  case 101:
    bg = 0xffff5555;
    break;
  case 102:
    bg = 0xff55ff55;
    break;
  case 103:
    bg = 0xffffff55;
    break;
  case 104:
    bg = 0xff5555ff;
    break;
  case 105:
    bg = 0xffff55ff;
    break;
  case 106:
    bg = 0xff55ffff;
    break;
  case 107:
    bg = 0xffffffff;
    break;
  }
}

static void handle_ansi(const char *seq) {
  if (seq[0] != '[')
    return;

  int nums[8];
  int count = 0;

  int value = 0;
  bool number = false;

  for (size_t i = 1; seq[i]; i++) {

    char c = seq[i];

    if (c >= '0' && c <= '9') {
      value = value * 10 + (c - '0');
      number = true;
    }

    else if (c == ';') {
      if (number) {
        if (count < 8)
          nums[count++] = value;
        value = 0;
        number = false;
      }
    }

    else if (c == 'K') {
      if (number)
        clear_line(value);
      else
        clear_line(0);

      flush_dirty();
      return;
    }

    else if (c == 'C') { // cursor forward
      uint32_t n = number ? (uint32_t)value : 1;
      uint32_t delta = n * font->width * scale;

      uint32_t old_x = cursor_x;
      cursor_x += delta;

      if (cursor_x > screen_width)
        cursor_x = screen_width;

      uint32_t w =
          (cursor_x >= old_x) ? (cursor_x - old_x + font->width * scale) : (font->width * scale);
      mark_dirty(old_x, cursor_y, w, font->height * scale);
      flush_dirty();
      return;
    }

    else if (c == 'D') { // cursor back
      uint32_t n = number ? (uint32_t)value : 1;
      uint32_t delta = n * font->width * scale;

      uint32_t old_x = cursor_x;
      cursor_x = (delta > cursor_x) ? 0 : cursor_x - delta;

      uint32_t w =
          (old_x >= cursor_x) ? (old_x - cursor_x + font->width * scale) : (font->width * scale);
      mark_dirty(cursor_x, cursor_y, w, font->height * scale);
      flush_dirty();
      return;
    }

    else if (c == 'm') {
      if (number && count < 8)
        nums[count++] = value;

      for (int j = 0; j < count; j++)
        ansi_code(nums[j]);

      return;
    }

    else if (c == 'J') {
      if (value == 2)
        clear();

      return;
    }

    else if (c == 'H') {
      mark_dirty(cursor_x, cursor_y, font->width, font->height);
      cursor_x = 0;
      cursor_y = 0;
      mark_dirty(cursor_x, cursor_y, font->width, font->height);
      flush_dirty();
      return;
    }
  }
}

void put(char c) {

  if (!font)
    return;

  // Erase any visible cursor block before drawing/editing anything,
  // so the blink loop never leaves a stale block behind.
  if (cursor_visible) {
    draw_cursor_block(false);
    cursor_visible = false;
    cursor_blink_counter = 0;
  }

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

    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || ansi_pos >= sizeof(ansi) - 1) {

      ansi[ansi_pos] = 0;

      handle_ansi(ansi);

      escape = false;
      ansi_pos = 0;
    }

    flush_dirty();
    return;
  }

  if (c == '\n') {

    newline();
    flush_dirty();
    return;
  }

  if (c == '\r') {
    cursor_x = 0;
    return;
  }

  if (c == 0x08 || c == 127) {
    backspace();
    return;
  }

  if (c == '\t') {
    cursor_x += 4 * font->width * scale;
    flush_dirty();
    return;
  }

  if (cursor_x + font->width * scale >= screen_width)
    newline();

  draw_char(c);

  flush_dirty();
}

void backspace() {
  if (cursor_visible) {
    draw_cursor_block(false);
    cursor_visible = false;
    cursor_blink_counter = 0;
  }

  uint32_t old_x = cursor_x;
  uint32_t old_y = cursor_y;

  if (cursor_x >= font->width * scale) {
    cursor_x -= font->width * scale;
  } else if (cursor_y >= font->height * scale) {
    cursor_y -= font->height * scale;
    cursor_x = ((screen_width / (font->width * scale)) - 1) * (font->width * scale);
  } else {
    return;
  }

  for (uint32_t y = 0; y < font->height * scale; y++) {
    for (uint32_t x = 0; x < font->width * scale; x++) {
      framebuffer::put_pixel(cursor_x + x, cursor_y + y, bg);
    }
  }

  mark_dirty(cursor_x, cursor_y, font->width * scale, font->height * scale);
  mark_dirty(old_x, old_y, font->width * scale, font->height * scale);
  flush_dirty();
}

void put_swap(char c) {
  lock();

  put(c);

  unlock();
}

void write(const char *buf, size_t len) {
  lock();

  for (size_t i = 0; i < len; i++)
    put(buf[i]);

  unlock();
}

void clear() {

  framebuffer::clear(bg);

  cursor_x = 0;
  cursor_y = 0;

  mark_dirty(0, 0, screen_width, screen_height);
  flush_dirty();
}

void set_font(psf::Font *f) {
  font = f;
}

// Call once per timer tick (e.g. from your PIT/APIC IRQ handler) to
// drive the blinking cursor while the console is otherwise idle.
void cursor_tick() {
  if (!font)
    return;

  cursor_blink_counter++;

  if (cursor_blink_counter >= CURSOR_BLINK_PERIOD) {
    cursor_blink_counter = 0;
    cursor_visible = !cursor_visible;

    draw_cursor_block(cursor_visible);
    flush_dirty();
  }
}

} // namespace console
