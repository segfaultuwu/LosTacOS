#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/fs/tarfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/paging.hpp"
#include <multiboot.h>
#include <stdint.h>
#include <string.h>

namespace multiboot2 {

char boot_cmdline[256] = "BOOT_IMAGE=/boot/kernel.elf quiet";

static uint32_t rootfs_start = 0;
static uint32_t rootfs_size = 0;

const char *get_bootloader_name(uint64_t addr) {
  auto *tag = (multiboot_tag *)((uint8_t *)addr + 8);

  while (tag->type != 0) {
    if (tag->type == TAG_BOOT_LOADER_NAME) {
      auto *name = (multiboot_tag_boot_loader_name *)tag;

      return name->name;
    }

    tag = (multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
  }

  return "unknown";
}

void parse_info(uint64_t mbi_phys_addr) {
  for_each_tag(mbi_phys_addr, [](struct multiboot_tag *tag) {
    switch (tag->type) {
    case 1: {
      struct multiboot_tag_string *cmd = (struct multiboot_tag_string *)tag;
      strncpy(boot_cmdline, cmd->string, sizeof(boot_cmdline) - 1);
      break;
    }
    case 3: {
      struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;

      uint32_t start = mod->mod_start;
      uint32_t end = mod->mod_end;
      uint32_t size = end - start;
      char *name = mod->cmdline;
      kprintf("Found module: %s | Start: 0x%x | Size: %d bytes\n", name, start, size);
      drivers::serial::writef("Found module: %s | Start: 0x%x | Size: %d bytes\n", name, start,
                              size);

      paging::reserve_below(end);
      if (mod->cmdline && strstr(mod->cmdline, "rootfs.tar")) {
        rootfs_start = mod->mod_start;
        rootfs_size = size;
      }
      break;
    }
    case 8: {
      framebuffer::init((uint64_t)tag);
      break;
    }
    }
  });
}

void mount_rootfs() {
  if (rootfs_start && rootfs_size) {
    fs::tarfs::mount((void *)(uintptr_t)rootfs_start, rootfs_size);
  }
}

int list_modules(uint64_t mbi_phys_addr, struct multiboot_module *out, int max_count) {
  int count = 0;

  for_each_tag(mbi_phys_addr, [&](struct multiboot_tag *tag) {
    if (tag->type != 3 || count >= max_count)
      return;

    struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;

    out[count].start = mod->mod_start;
    out[count].end = mod->mod_end;
    out[count].cmdline = mod->cmdline;

    count++;
  });

  return count;
}

} // namespace multiboot2
