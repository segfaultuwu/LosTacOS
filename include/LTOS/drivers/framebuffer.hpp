#pragma once

#include <stdint.h>

namespace framebuffer {

struct Info {
  uint8_t *address;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;
};

extern Info info;

namespace grub {
bool init(uint64_t addr, Info *info);
bool available();
} // namespace grub

namespace limine {
bool init(uint64_t addr, Info *info);
bool available();
} // namespace limine

char *get_bootloader();

void init(uint64_t addr);
void init_backbuffer();

void put_pixel(int x, int y, uint32_t color);
void put_pixel_unchecked(int x, int y, uint32_t color);
uint32_t get_pixel(int x, int y);
void fill_rect(int x, int y, int w, int h, uint32_t color);
void clear(uint32_t color);
void swap();
void swap_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

uint32_t get_width();
uint32_t get_height();
uint32_t get_pitch();
uint32_t get_bpp();

uint8_t *get_address();
uint8_t *get_backbuffer();
uint8_t *get_frontbuffer();

bool is_fb_phys_address(uint64_t phys);

} // namespace framebuffer

