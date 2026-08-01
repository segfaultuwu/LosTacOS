#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "multiboot.h"

#include <stdint.h>

extern uint32_t mb_magic;
extern uint32_t mb_info;

namespace framebuffer::grub {

bool available() {
  return mb_magic == 0x36d76289;
}

bool init(uint64_t addr, Info *info) {
  if (!available() || !info)
    return false;

  uint8_t *ptr = reinterpret_cast<uint8_t *>((uintptr_t)mb_info);

  // skip multiboot info header
  ptr += 8;

  while (true) {
    auto *tag = reinterpret_cast<multiboot_tag *>(ptr);

    if (tag->type == 0)
      break;

    if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
      auto *fb = reinterpret_cast<multiboot_tag_framebuffer_common *>(tag);

      if (!fb->framebuffer_addr)
        return false;

      info->address = reinterpret_cast<uint8_t *>((uintptr_t)fb->framebuffer_addr);

      info->width = fb->framebuffer_width;

      info->height = fb->framebuffer_height;

      info->pitch = fb->framebuffer_pitch;

      info->bpp = fb->framebuffer_bpp;

      drivers::serial::writef("FB [GRUB]: %ux%u pitch=%u bpp=%u addr=%lx\n", info->width,
                              info->height, info->pitch, info->bpp, (uint64_t)info->address);

      return true;
    }

    // Multiboot2 tags są wyrównane do 8 bajtów
    ptr += (tag->size + 7) & ~7;
  }

  return false;
}

} // namespace framebuffer::grub
