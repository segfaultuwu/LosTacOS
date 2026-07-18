#include "LTOS/drivers/psf.hpp"
#include "LTOS/lib/kprintf.h"
#include "multiboot.h"

#include <stdint.h>
#include <string.h>

namespace psf {

static Font current_font{};
static bool loaded = false;

struct PSF1_Header {
  uint8_t magic[2];
  uint8_t mode;
  uint8_t charsize;
};

bool load(void *data, size_t size, Font *font) {
  if (!data || !font)
    return false;

  if (size < sizeof(PSF1_Header))
    return false;

  auto *hdr = (PSF1_Header *)data;

  if (hdr->magic[0] != 0x36 || hdr->magic[1] != 0x04) {
    return false;
  }

  uint32_t count = (hdr->mode & 1) ? 512 : 256;

  uint8_t *glyphs = (uint8_t *)data + sizeof(PSF1_Header);

  size_t required = sizeof(PSF1_Header) + count * hdr->charsize;

  if (required > size)
    return false;

  font->glyphs = glyphs;
  font->glyph_count = count;
  font->height = hdr->charsize;
  font->width = 8;

  return true;
}

uint8_t *get_glyph(Font *font, uint8_t c) {
  if (!font)
    return nullptr;

  if (!font->glyphs)
    return nullptr;

  if (c >= font->glyph_count)
    c = 0;

  return font->glyphs + ((uint32_t)c * font->height);
}

void find_font(uint64_t mbi) {
  kprintf("Searching PSF...\n");

  multiboot2::for_each_tag(mbi, [](multiboot_tag *tag) {
    kprintf("TAG %d\n", tag->type);

    if (tag->type != 3)
      return;

    auto *mod = (multiboot_tag_module *)tag;

    kprintf("MOD: '%s'\n", mod->cmdline);

    if (!*mod->cmdline)
      return;

    if (strcmp(mod->cmdline, "font.psf") != 0)
      return;

    size_t size = mod->mod_end - mod->mod_start;

    kprintf("Loading PSF addr=%x size=%d\n", mod->mod_start, size);

    if (load((void *)mod->mod_start, size, &current_font)) {

      loaded = true;

      kprintf("PSF OK width=%d height=%d glyphs=%d\n", current_font.width,
              current_font.height, current_font.glyph_count);

    } else {

      kprintf("PSF LOAD FAILED\n");
    }
  });
}

Font *get() {
  if (!loaded)
    return nullptr;

  return &current_font;
}

} // namespace psf
