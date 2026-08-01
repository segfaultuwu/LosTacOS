#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/boot.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/panic.hpp"
#include <stdint.h>
#include <string.h>

namespace framebuffer {

static uint8_t *frontbuffer;
static uint8_t *backbuffer;
static uint32_t fb_size;

static bool swapping = false;

Info info{};
char bootloader[16];

void init(uint64_t addr) {
  // Autodetect Limine vs GRUB framebuffer initialization
  bool ok = false;

  if (boot_info.bootloader == Bootloader::Limine) {
    framebuffer::limine::init(addr, &info);
    ok = true;
  } else if (boot_info.bootloader == Bootloader::Grub) {
    framebuffer::grub::init(addr, &info);
    ok = true;
  }

  if (!ok) {
    panic::halt("No framebuffer");
  }

  if (!ok) {
    drivers::serial::write("FB: failed to initialize framebuffer from both Limine and GRUB\n");
    return;
  }

  frontbuffer = info.address;
  fb_size = info.pitch * info.height;
}

// Yes.
char *get_bootloader() {
  return bootloader;
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

uint32_t get_pixel(int x, int y) {
  if (!backbuffer || x < 0 || y < 0 || x >= (int)info.width || y >= (int)info.height)
    return 0;

  uint8_t *pixel = backbuffer + y * info.pitch + x * (info.bpp / 8);

  switch (info.bpp) {
  case 32:
    return *(uint32_t *)pixel;
  case 24:
    return (uint32_t)pixel[0] | ((uint32_t)pixel[1] << 8) | ((uint32_t)pixel[2] << 16);
  case 16:
    return *(uint16_t *)pixel;
  default:
    return 0;
  }
}

uint8_t *get_backbuffer() {
  return backbuffer;
}

uint8_t *get_frontbuffer() {
  return frontbuffer;
}

void swap_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
  uint32_t pitch = get_pitch();
  uint8_t *back = get_backbuffer();
  uint8_t *front = get_frontbuffer();

  uint32_t bytes_per_px = pitch / get_width();

  for (uint32_t row = y; row < y + h && row < get_height(); row++) {
    uint32_t off = row * pitch + x * bytes_per_px;
    uint32_t len = w * bytes_per_px;
    memcpy(front + off, back + off, len);
  }
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
