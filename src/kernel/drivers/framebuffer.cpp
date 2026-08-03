#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/boot.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/paging.hpp"
#include "LTOS/panic.hpp"
#include <stdint.h>
#include <string.h>

namespace framebuffer {

static uint8_t *frontbuffer;
static uint8_t *backbuffer;
static uint32_t fb_size;

static bool swapping = false;
static uint32_t bytes_per_px;

static void (*put_pixel_impl)(int x, int y, uint32_t color) = nullptr;
static uint32_t (*get_pixel_impl)(int x, int y) = nullptr;

Info info{};
char bootloader[16];

static void put_pixel_32(int x, int y, uint32_t color) {
  *(uint32_t *)(backbuffer + y * info.pitch + x * 4) = color;
}

static void put_pixel_24(int x, int y, uint32_t color) {
  uint8_t *p = backbuffer + y * info.pitch + x * 3;
  p[0] = color & 0xff;
  p[1] = (color >> 8) & 0xff;
  p[2] = (color >> 16) & 0xff;
}

static void put_pixel_16(int x, int y, uint32_t color) {
  uint16_t rgb565 =
      (((color >> 19) & 0x1f) << 11) | (((color >> 10) & 0x3f) << 5) | ((color >> 3) & 0x1f);
  *(uint16_t *)(backbuffer + y * info.pitch + x * 2) = rgb565;
}

static uint32_t get_pixel_32(int x, int y) {
  return *(uint32_t *)(backbuffer + y * info.pitch + x * 4);
}

static uint32_t get_pixel_24(int x, int y) {
  uint8_t *p = backbuffer + y * info.pitch + x * 3;
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t get_pixel_16(int x, int y) {
  return *(uint16_t *)(backbuffer + y * info.pitch + x * 2);
}

void init(uint64_t addr) {
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

  frontbuffer = info.address;
  fb_size = info.pitch * info.height;
  bytes_per_px = info.bpp / 8;

  switch (info.bpp) {
  case 32:
    put_pixel_impl = put_pixel_32;
    get_pixel_impl = get_pixel_32;
    break;
  case 24:
    put_pixel_impl = put_pixel_24;
    get_pixel_impl = get_pixel_24;
    break;
  case 16:
    put_pixel_impl = put_pixel_16;
    get_pixel_impl = get_pixel_16;
    break;
  }

  if (frontbuffer && fb_size > 0) {
    uint64_t virt = (uint64_t)frontbuffer;
    uint64_t phys = (virt >= 0xffff800000000000ULL) ? (virt - 0xffff800000000000ULL) : virt;
    paging::map_range(virt, phys, fb_size, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  }
}

char *get_bootloader() {
  return bootloader;
}

bool is_fb_phys_address(uint64_t phys) {
  if (!info.address || !fb_size)
    return false;
  uint64_t fb_phys = (uint64_t)info.address;
  if (fb_phys >= 0xffff800000000000ULL)
    fb_phys -= 0xffff800000000000ULL;
  return (phys >= fb_phys && phys < fb_phys + fb_size);
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
  if (!backbuffer || x < 0 || y < 0 || x >= (int)info.width || y >= (int)info.height)
    return;

  put_pixel_impl(x, y, color);
}

void put_pixel_unchecked(int x, int y, uint32_t color) {
  put_pixel_impl(x, y, color);
}

uint32_t get_pixel(int x, int y) {
  if (!backbuffer || x < 0 || y < 0 || x >= (int)info.width || y >= (int)info.height)
    return 0;

  return get_pixel_impl(x, y);
}

void fill_rect(int x, int y, int w, int h, uint32_t color) {
  if (!backbuffer || w <= 0 || h <= 0)
    return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > (int)info.width) w = info.width - x;
  if (y + h > (int)info.height) h = info.height - y;
  if (w <= 0 || h <= 0)
    return;

  if (info.bpp == 32) {
    for (int row = 0; row < h; row++) {
      uint32_t *ptr = (uint32_t *)(backbuffer + (y + row) * info.pitch + x * 4);
      for (int col = 0; col < w; col++)
        ptr[col] = color;
    }
  } else {
    for (int row = 0; row < h; row++)
      for (int col = 0; col < w; col++)
        put_pixel_impl(x + col, y + row, color);
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

  if (info.bpp == 32) {
    uint64_t double_pixel = ((uint64_t)color << 32) | color;

    for (uint32_t y = 0; y < info.height; y++) {
      uint64_t *row = (uint64_t *)(backbuffer + y * info.pitch);
      uint32_t count = info.width;

      while (count >= 2) {
        *row++ = double_pixel;
        count -= 2;
      }

      if (count)
        *(uint32_t *)row = color;
    }
  } else {
    for (uint32_t y = 0; y < info.height; y++)
      for (uint32_t x = 0; x < info.width; x++)
        put_pixel_impl(x, y, color);
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
