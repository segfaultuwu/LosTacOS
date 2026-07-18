#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/serial.hpp"
#include "multiboot.h"
#include <cstdint>

namespace framebuffer {

Info info{};

void init(uint64_t addr) {
  auto *fb = reinterpret_cast<multiboot_tag_framebuffer_common *>(addr);

  info.address =
      reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(fb->framebuffer_addr));
  info.width = fb->framebuffer_width;
  info.height = fb->framebuffer_height;
  info.pitch = fb->framebuffer_pitch;
  info.bpp = fb->framebuffer_bpp;

  drivers::serial::writef("Framebuffer:\n"
                          " addr=%lx\n"
                          " pitch=%u\n"
                          " width=%u\n"
                          " height=%u\n"
                          " bpp=%u\n"
                          " type=%u\n",
                          fb->framebuffer_addr, fb->framebuffer_pitch,
                          fb->framebuffer_width, fb->framebuffer_height,
                          fb->framebuffer_bpp, fb->framebuffer_type);
}

void put_pixel(int x, int y, uint32_t color) {
  if (!info.address)
    return;

  if (x < 0 || y < 0)
    return;

  if (x >= (int)info.width || y >= (int)info.height)
    return;

  uint8_t *pixel = info.address + y * info.pitch + x * (info.bpp / 8);

  switch (info.bpp) {

  case 32:
    *(uint32_t *)pixel = color;
    break;

  case 24:
    pixel[0] = (color >> 0) & 0xff;
    pixel[1] = (color >> 8) & 0xff;
    pixel[2] = (color >> 16) & 0xff;
    break;

  case 16: {
    uint16_t rgb565 = (((color >> 19) & 0x1f) << 11) |
                      (((color >> 10) & 0x3f) << 5) | ((color >> 3) & 0x1f);

    *(uint16_t *)pixel = rgb565;
    break;
  }

  default:
    break;
  }
}

void clear(uint32_t color) {
  if (!info.address)
    return;

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

uint8_t *get_address() { return info.address; }

} // namespace framebuffer
