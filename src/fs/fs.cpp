#include "LTOS/fs/fs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include <string.h>

namespace fs {

static FileSystem *filesystems[16];
static int fs_count = 0;

void register_filesystem(FileSystem *fs) {
  if (!fs)
    return;

  for (int i = 0; i < fs_count; i++) {
    if (filesystems[i] == fs)
      return;
  }

  if (fs_count >= 16)
    return;

  filesystems[fs_count++] = fs;
}

FileSystem *get_filesystem(const char *name) {
  if (!name)
    return nullptr;

  for (int i = 0; i < fs_count; i++) {
    if (filesystems[i]->name && strcmp(filesystems[i]->name, name) == 0)
      return filesystems[i];
  }

  return nullptr;
}

bool mount_fs(const char *path, FileSystem *filesystem, void *data) {
  return mount(path, filesystem, data);
}

} // namespace fs

