#include "LTOS/drivers/console.hpp"

#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/psf.hpp"

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

uint32_t get_rows() {
  if (!font)
    return 25;
  uint32_t fh = font->height * scale;
  return fh > 0 ? (screen_height / fh) : 25;
}

uint32_t get_cols() {
  if (!font)
    return 80;
  uint32_t fw = font->width * scale;
  return fw > 0 ? (screen_width / fw) : 80;
}

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
  if (!font)
    return;

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

  framebuffer::fill_rect(start, cursor_y, end - start, font->height * scale, bg);

  mark_dirty(start, cursor_y, end - start, font->height * scale);
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
      framebuffer::put_pixel_unchecked(cursor_x + x, cursor_y + y, px ^ 0x00FFFFFF);
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

  framebuffer::swap();
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
        framebuffer::put_pixel_unchecked(cursor_x + x, cursor_y + row, color);
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

  uint32_t line_h = font->height * scale;

  if (screen_height <= line_h)
    return;

  uint32_t move = pitch * (screen_height - line_h);

  memmove(fb, fb + pitch * line_h, move);

  memset(fb + move, 0, pitch * line_h);

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
  if (!font)
    return;

  // Private mode sequences (e.g. \033[?25h, \033[?25l)
  if (seq[0] == '[' && seq[1] == '?') {
    int mode_val = 0;
    size_t idx = 2;
    while (seq[idx] >= '0' && seq[idx] <= '9') {
      mode_val = mode_val * 10 + (seq[idx] - '0');
      idx++;
    }
    if (mode_val == 25) {
      if (seq[idx] == 'h') {
        cursor_visible = true;
      } else if (seq[idx] == 'l') {
        if (cursor_visible) {
          draw_cursor_block(false);
          cursor_visible = false;
        }
      }
    }
    return;
  }

  if (seq[0] != '[')
    return;

  int nums[8] = {0};
  int count = 0;
  int value = 0;
  bool has_num = false;

  size_t i = 1;
  while (seq[i]) {
    char c = seq[i];

    if (c >= '0' && c <= '9') {
      value = value * 10 + (c - '0');
      has_num = true;
    } else if (c == ';') {
      if (count < 8) {
        nums[count++] = has_num ? value : 0;
      }
      value = 0;
      has_num = false;
    } else {
      if (has_num && count < 8) {
        nums[count++] = value;
      }

      uint32_t char_w = font->width * scale;
      uint32_t char_h = font->height * scale;
      uint32_t max_cols = (char_w > 0) ? (screen_width / char_w) : 80;
      uint32_t max_rows = (char_h > 0) ? (screen_height / char_h) : 25;

      switch (c) {
      case 'H':
      case 'f': {
        int r = (count >= 1 && nums[0] > 0) ? nums[0] - 1 : 0;
        int col = (count >= 2 && nums[1] > 0) ? nums[1] - 1 : 0;

        if ((uint32_t)r >= max_rows)
          r = (int)max_rows - 1;
        if ((uint32_t)col >= max_cols)
          col = (int)max_cols - 1;
        if (r < 0)
          r = 0;
        if (col < 0)
          col = 0;

        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        cursor_y = r * char_h;
        cursor_x = col * char_w;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        flush_dirty();
        return;
      }

      case 'A': {
        int n = (count >= 1 && nums[0] > 0) ? nums[0] : 1;
        uint32_t delta = n * char_h;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        cursor_y = (delta > cursor_y) ? 0 : cursor_y - delta;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        flush_dirty();
        return;
      }

      case 'B': {
        int n = (count >= 1 && nums[0] > 0) ? nums[0] : 1;
        uint32_t delta = n * char_h;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        cursor_y += delta;
        if (cursor_y >= screen_height)
          cursor_y = screen_height - char_h;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        flush_dirty();
        return;
      }

      case 'C': {
        int n = (count >= 1 && nums[0] > 0) ? nums[0] : 1;
        uint32_t delta = n * char_w;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        cursor_x += delta;
        if (cursor_x >= screen_width)
          cursor_x = screen_width - char_w;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        flush_dirty();
        return;
      }

      case 'D': {
        int n = (count >= 1 && nums[0] > 0) ? nums[0] : 1;
        uint32_t delta = n * char_w;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        cursor_x = (delta > cursor_x) ? 0 : cursor_x - delta;
        mark_dirty(cursor_x, cursor_y, char_w, char_h);
        flush_dirty();
        return;
      }

      case 'K': {
        int mode = (count >= 1) ? nums[0] : 0;
        clear_line(mode);
        flush_dirty();
        return;
      }

      case 'J': {
        int mode = (count >= 1) ? nums[0] : 0;
        if (mode == 2) {
          clear();
        }
        flush_dirty();
        return;
      }

      case 'm': {
        if (count == 0) {
          ansi_code(0);
        } else {
          for (int j = 0; j < count; j++)
            ansi_code(nums[j]);
        }
        return;
      }
      }
      return;
    }
    i++;
  }
}

void put(char c) {
  if (!font)
    return;

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
