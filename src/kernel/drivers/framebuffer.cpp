#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "multiboot.h"
#include <cstdint>

namespace framebuffer {

Info info;

void init(uint64_t addr) {
  auto *fb = (multiboot_tag_framebuffer_common *)addr;

  info.address = (uint32_t *)fb->framebuffer_addr;

  info.width = fb->framebuffer_width;

  info.height = fb->framebuffer_height;

  info.pitch = fb->framebuffer_pitch;

  info.bpp = fb->framebuffer_bpp;

  drivers::serial::writef("Framebuffer addr: %lx width=%u height=%u\n",
                          fb->framebuffer_addr, fb->framebuffer_width,
                          fb->framebuffer_height);
}

void put_pixel(int x, int y, uint32_t color) {
  if (x < 0 || y < 0)
    return;

  if (x >= (int)info.width || y >= (int)info.height)
    return;

  uint32_t *pixel =
      (uint32_t *)((uint8_t *)info.address + y * info.pitch + x * 4);

  *pixel = color;
}

void clear(uint32_t color) {
  for (uint32_t y = 0; y < info.height; y++) {
    for (uint32_t x = 0; x < info.width; x++) {
      put_pixel(x, y, color);
    }
  }
}

uint32_t get_width() { return info.width; }

uint32_t get_height() { return info.height; }

uint32_t get_pitch() { return info.pitch; }

uint32_t get_bpp() { return info.bpp; }

uint32_t *get_address() { return info.address; }

} // namespace framebuffer
