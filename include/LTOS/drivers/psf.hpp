#pragma once

#include <stddef.h>
#include <stdint.h>

namespace psf {

struct Font {
  uint8_t *glyphs;

  uint32_t glyph_count;
  uint32_t width;
  uint32_t height;
};

bool load(void *data, size_t size, Font *font);

uint8_t *get_glyph(Font *font, uint8_t c);

void find_font(uint64_t mbi);

Font *get();

} // namespace psf
