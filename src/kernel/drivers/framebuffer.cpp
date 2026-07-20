#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/mm/heap.hpp"
#include "multiboot.h"
#include <cstdint>
#include <string.h>

namespace framebuffer {

static uint8_t *frontbuffer;
static uint8_t *backbuffer;
static uint32_t fb_size;

static bool swapping = false;

Info info{};

void init(uint64_t addr) {
  auto *fb = reinterpret_cast<multiboot_tag_framebuffer_common *>(addr);

  info.address = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(fb->framebuffer_addr));

  info.width = fb->framebuffer_width;
  info.height = fb->framebuffer_height;
  info.pitch = fb->framebuffer_pitch;
  info.bpp = fb->framebuffer_bpp;

  frontbuffer = info.address;

  fb_size = info.pitch * info.height;
}

void init_backbuffer() {
  backbuffer = (uint8_t *)heap::kmalloc(fb_size);

  if (!backbuffer) {
    drivers::serial::write("FB: backbuffer alloc failed\n");
    return;
  }

  memset(backbuffer, 0, fb_size);

  drivers::serial::writef("FB backbuffer=%lx size=%u\n", (uint64_t)backbuffer, fb_size);
}

void put_pixel(int x, int y, uint32_t color) {
  if (!backbuffer)
    return;

  if (x < 0 || y < 0)
    return;

  if (x >= (int)info.width || y >= (int)info.height)
    return;

  uint8_t *pixel = backbuffer + y * info.pitch + x * (info.bpp / 8);

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
    uint16_t rgb565 =
        (((color >> 19) & 0x1f) << 11) | (((color >> 10) & 0x3f) << 5) | ((color >> 3) & 0x1f);

    *(uint16_t *)pixel = rgb565;
    break;
  }

  default:
    break;
  }
}

uint8_t *get_backbuffer() {
  return backbuffer;
}

void swap() {
  if (swapping)
    return;

  swapping = true;

  memcpy(frontbuffer, backbuffer, fb_size);

  swapping = false;
}

void clear(uint32_t color) {
  if (!backbuffer)
    return;

  for (uint32_t y = 0; y < info.height; y++) {
    uint32_t *row = (uint32_t *)(backbuffer + y * info.pitch);

    for (uint32_t x = 0; x < info.width; x++) {
      row[x] = color;
    }
  }
}

uint32_t get_width() {
  return info.width;
}

uint32_t get_height() {
  return info.height;
}

uint32_t get_pitch() {
  return info.pitch;
}

uint32_t get_bpp() {
  return info.bpp;
}

uint8_t *get_address() {
  return info.address;
}

} // namespace framebuffer
