#pragma once

#include <cstdint>

namespace framebuffer {

struct Info {
  uint8_t *address;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;
};

extern Info info;

void init(uint64_t addr);
void init_backbuffer();

void put_pixel(int x, int y, uint32_t color);
void clear(uint32_t color);
void swap();

uint32_t get_width();
uint32_t get_height();
uint32_t get_pitch();
uint32_t get_bpp();

uint8_t *get_address();
uint8_t *get_backbuffer();

} // namespace framebuffer
