#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "multiboot.h"
#include <stdint.h>

namespace framebuffer::limine {

struct LimineFramebuffer {
  void *address;
  uint64_t width;
  uint64_t height;
  uint64_t pitch;
  uint16_t bpp;
  uint8_t memory_model;
  uint8_t red_mask_size;
  uint8_t red_mask_shift;
  uint8_t green_mask_size;
  uint8_t green_mask_shift;
  uint8_t blue_mask_size;
  uint8_t blue_mask_shift;
};

struct LimineFramebufferResponse {
  uint64_t revision;
  uint64_t framebuffer_count;
  LimineFramebuffer **framebuffers;
};

struct LimineFramebufferRequest {
  uint64_t id[4];
  uint64_t revision;
  LimineFramebufferResponse *response;
};

// Limine framebuffer request tag ID
__attribute__((used, section(".requests"))) static volatile LimineFramebufferRequest fb_request = {
    .id = {0xc9b8d70d74204d1c, 0x35d1d90df9707532, 0x0d0d1e57692120e2, 0xb8003a89e6e440e5},
    .revision = 0,
    .response = nullptr};

bool available() {
  return fb_request.response != nullptr;
}

bool init(uint64_t addr, Info *info) {
  if (!info)
    return false;

  // 1. Try native Limine framebuffer request
  if (fb_request.response && fb_request.response->framebuffer_count > 0) {
    LimineFramebuffer *fb = fb_request.response->framebuffers[0];
    if (fb && fb->address) {
      info->address = reinterpret_cast<uint8_t *>(fb->address);
      info->width = (uint32_t)fb->width;
      info->height = (uint32_t)fb->height;
      info->pitch = (uint32_t)fb->pitch;
      info->bpp = (uint32_t)fb->bpp;

      drivers::serial::writef("FB [Limine Native]: %ux%u pitch=%u bpp=%u addr=%lx\n", info->width,
                              info->height, info->pitch, info->bpp, (uint64_t)info->address);
      return true;
    }
  }

  // 2. Try Multiboot2 tag passed by Limine
  if (addr) {
    auto *fb = reinterpret_cast<multiboot_tag_framebuffer_common *>(addr);
    if (fb->framebuffer_addr && fb->framebuffer_width > 0) {
      info->address = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(fb->framebuffer_addr));
      info->width = fb->framebuffer_width;
      info->height = fb->framebuffer_height;
      info->pitch = fb->framebuffer_pitch;
      info->bpp = fb->framebuffer_bpp;

      drivers::serial::writef("FB [Limine Multiboot2]: %ux%u pitch=%u bpp=%u addr=%lx\n",
                              info->width, info->height, info->pitch, info->bpp,
                              (uint64_t)info->address);
      return true;
    }
  }

  return false;
}

} // namespace framebuffer::limine
