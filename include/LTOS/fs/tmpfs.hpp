#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fs::tmpfs {

struct TmpFile {
  uint8_t *data;
  size_t size;
};

} // namespace fs::tmpfs
