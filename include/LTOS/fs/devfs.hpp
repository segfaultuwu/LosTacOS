#pragma once

#include "LTOS/fs/vfs.hpp"

namespace fs::devfs {

using DevOps = fs::vfs::DevOps;

void init();

void register_device(const char *name, DevOps *ops);

} // namespace fs::devfs
