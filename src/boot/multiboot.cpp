#include "LTOS/lib/kprintf.h"
#include <cstdint>
#include <multiboot.h>

namespace multiboot2 {

namespace {
template <typename Fn> void for_each_tag(uint64_t mbi_phys_addr, Fn fn) {
  uint8_t *mbi = (uint8_t *)mbi_phys_addr;

  uint32_t total_size = *(uint32_t *)mbi;
  struct multiboot_tag *tag = (struct multiboot_tag *)(mbi + 8);

  while (tag->type != 0 && (uint8_t *)tag < mbi + total_size) {
    fn(tag);

    tag = (struct multiboot_tag *)((uint8_t *)tag +
                                   ((tag->size + 7) & ~7)); // align to 8
  }
}
} // namespace

void parse_info(uint64_t mbi_phys_addr) {
  for_each_tag(mbi_phys_addr, [](struct multiboot_tag *tag) {
    if (tag->type == 3) {
      struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;

      uint32_t start = mod->mod_start;
      uint32_t end = mod->mod_end;
      uint32_t size = end - start;
      char *name = mod->cmdline;
      kprintf("Found module: %s | Start: 0x%x | Size: %d bytes\n", name, start,
              size);
    }
  });
}

int list_modules(uint64_t mbi_phys_addr, struct multiboot_module *out,
                 int max_count) {
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
