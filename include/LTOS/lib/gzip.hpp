#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gzip {

bool is_gzip(const void *data, size_t size);

// Returns true on success. Allocates or fills dest with uncompressed data.
bool decompress(const void *src, size_t src_size, void *dest, size_t *dest_size);

} // namespace gzip
