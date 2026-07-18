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

struct PSF2_Header {
  uint32_t magic;
  uint32_t version;
  uint32_t headersize;
  uint32_t flags;
  uint32_t glyph_count;
  uint32_t glyph_size;
  uint32_t height;
  uint32_t width;
};

static constexpr uint32_t PSF2_MAGIC = 0x864ab572;

static bool load_psf1(void *data, size_t size, Font *font) {
  auto *hdr = (PSF1_Header *)data;

  uint32_t count = (hdr->mode & 1) ? 512 : 256;

  uint32_t glyph_size = hdr->charsize;

  size_t needed = sizeof(PSF1_Header) + count * glyph_size;

  if (needed > size)
    return false;

  font->glyphs = (uint8_t *)data + sizeof(PSF1_Header);

  font->glyph_count = count;
  font->width = 8;
  font->height = hdr->charsize;

  font->bytes_per_row = 1;
  font->glyph_size = glyph_size;

  return true;
}

static bool load_psf2(void *data, size_t size, Font *font) {
  auto *hdr = (PSF2_Header *)data;

  kprintf("PSF2 hdr=%d glyphs=%d glyph_size=%d width=%d height=%d\n",
          hdr->headersize, hdr->glyph_count, hdr->glyph_size, hdr->width,
          hdr->height);

  if (hdr->magic != PSF2_MAGIC)
    return false;

  size_t needed = hdr->headersize + hdr->glyph_count * hdr->glyph_size;

  if (needed > size)
    return false;

  font->glyphs = (uint8_t *)data + hdr->headersize;

  font->glyph_count = hdr->glyph_count;
  font->width = hdr->width;
  font->height = hdr->height;

  font->glyph_size = hdr->glyph_size;
  font->bytes_per_row = (hdr->width + 7) / 8;

  return true;
}

bool load(void *data, size_t size, Font *font) {
  if (!data || !font)
    return false;

  *font = {};

  uint8_t *raw = (uint8_t *)data;

  kprintf("FONT MAGIC: %x %x %x %x\n", raw[0], raw[1], raw[2], raw[3]);

  if (raw[0] == 0x36 && raw[1] == 0x04) {
    kprintf("Detected PSF1\n");

    if (!load_psf1(data, size, font))
      return false;
  } else {
    uint32_t magic = raw[0] | (raw[1] << 8) | (raw[2] << 16) | (raw[3] << 24);

    if (magic != PSF2_MAGIC) {
      kprintf("Unknown PSF\n");
      return false;
    }

    kprintf("Detected PSF2\n");

    if (!load_psf2(data, size, font))
      return false;
  }

  kprintf("FONT OK: %dx%d glyphs=%d size=%d\n", font->width, font->height,
          font->glyph_count, font->glyph_size);

  return true;
}

uint8_t *get_glyph(Font *font, uint8_t c) {
  if (!font || !font->glyphs)
    return nullptr;

  if (c >= font->glyph_count)
    c = 0;

  return font->glyphs + ((uint32_t)c * font->glyph_size);
}

void find_font(uint64_t mbi) {
  kprintf("Searching PSF...\n");

  multiboot2::for_each_tag(mbi, [](multiboot_tag *tag) {
    if (tag->type != 3)
      return;

    auto *mod = (multiboot_tag_module *)tag;

    if (!mod->cmdline)
      return;

    if (strcmp(mod->cmdline, "font.psf") != 0)
      return;

    size_t size = mod->mod_end - mod->mod_start;

    kprintf("Loading PSF addr=%x size=%d\n", mod->mod_start, size);

    if (load((void *)mod->mod_start, size, &current_font)) {
      loaded = true;
      kprintf("PSF LOADED\n");
    } else {
      kprintf("PSF FAILED\n");
    }
  });
}

Font *get() { return loaded ? &current_font : nullptr; }

} // namespace psf
