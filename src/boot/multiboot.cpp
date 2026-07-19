#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/fs/tarfs.hpp"
#include "LTOS/lib/kprintf.h"
#include <cstdint>
#include <multiboot.h>
#include <string.h>

namespace multiboot2 {

void parse_info(uint64_t mbi_phys_addr) {
  for_each_tag(mbi_phys_addr, [](struct multiboot_tag *tag) {
    switch (tag->type) {
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
      if (strcmp(mod->cmdline, "rootfs.tar") == 0) {
        fs::tarfs::mount((void *)mod->mod_start, mod->mod_end - mod->mod_start);
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
