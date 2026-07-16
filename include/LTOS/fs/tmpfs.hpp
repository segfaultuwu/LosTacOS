#pragma once

#include <cstddef>
#include <cstdint>

namespace fs::tmpfs {

struct TmpFile {
  uint8_t *data;
  size_t size;
};

} // namespace fs::tmpfs
