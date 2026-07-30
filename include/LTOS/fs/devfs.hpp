#pragma once

#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"

namespace fs::devfs {

extern FileSystem filesystem;

bool init(FileSystem *fs);

void register_device(const char *name, vfs::DevOps *ops);

void init_null();
void init_fb();

} // namespace fs::devfs
