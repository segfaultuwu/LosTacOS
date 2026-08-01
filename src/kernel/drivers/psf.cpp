#include "LTOS/drivers/psf.hpp"
#include "LTOS/lib/gzip.hpp"
#include "LTOS/mm/heap.hpp"
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
  uint32_t header_size;
  uint32_t flags;
  uint32_t glyph_count;
  uint32_t glyph_size;
  uint32_t height;
  uint32_t width;
};

static constexpr uint32_t PSF2_MAGIC = 0x864ab572;

static bool load_psf1(void *data, size_t size, Font *font) {
  if (size < sizeof(PSF1_Header))
    return false;

  auto *hdr = (PSF1_Header *)data;

  font->glyphs = (uint8_t *)data + sizeof(PSF1_Header);

  if (hdr->mode & 0x01)
    font->glyph_count = 512;
  else
    font->glyph_count = 256;

  font->width = 8;
  font->height = hdr->charsize;

  font->glyph_size = hdr->charsize;
  font->bytes_per_row = 1;

  return true;
}

static bool load_psf2(void *data, size_t size, Font *font) {
  if (size < sizeof(PSF2_Header))
    return false;

  auto *hdr = (PSF2_Header *)data;

  font->glyphs = (uint8_t *)data + hdr->header_size;

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

  if (gzip::is_gzip(data, size)) {
    size_t out_size = 256 * 1024;
    void *uncompressed = heap::kmalloc(out_size);
    if (uncompressed && gzip::decompress(data, size, uncompressed, &out_size)) {
      data = uncompressed;
      size = out_size;
    }
  }

  uint8_t *raw = (uint8_t *)data;

  if (raw[0] == 0x36 && raw[1] == 0x04) {
    if (!load_psf1(data, size, font))
      return false;
  } else {
    uint32_t magic = raw[0] | (raw[1] << 8) | (raw[2] << 16) | (raw[3] << 24);

    if (magic != PSF2_MAGIC) {
      return false;
    }

    if (!load_psf2(data, size, font))
      return false;
  }

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
  multiboot2::for_each_tag(mbi, [](multiboot_tag *tag) {
    if (tag->type != 3)
      return;

    auto *mod = (multiboot_tag_module *)tag;

    if (!mod->cmdline)
      return;

    if (!::strstr(mod->cmdline, "font.psf"))
      return;

    size_t size = mod->mod_end - mod->mod_start;

    if (load((void *)(uintptr_t)mod->mod_start, size, &current_font)) {
      loaded = true;
    }
  });
}

Font *get() {
  if (!loaded)
    return nullptr;

  return &current_font;
}

void set_font(Font *f) {
  if (f) {
    current_font = *f;
    loaded = true;
  }
}

} // namespace psf
