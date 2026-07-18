#pragma once

#include <stddef.h>
#include <stdint.h>

namespace framebuffer {

struct Info {
  uint32_t *address;

  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint8_t bpp;
};

void init(uint64_t addr);

void put_pixel(int x, int y, uint32_t color);

void fill_rect(int x, int y, int w, int h, uint32_t color);

void clear(uint32_t color);

uint32_t get_width();
uint32_t get_height();

extern Info info;

} // namespace framebuffer
