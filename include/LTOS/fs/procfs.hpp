#pragma once

#include "LTOS/fs/fs.hpp"
#include <stddef.h>

namespace fs::procfs {

struct ProcFile {
  char *data;
  size_t offset;
  size_t size;
};

extern FileSystem filesystem;

} // namespace fs::procfs
