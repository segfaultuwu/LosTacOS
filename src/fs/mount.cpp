#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"

#include <string.h>

namespace fs {

static Mount *mounts = nullptr;

bool mount(const char *path, FileSystem *fs, void *data) {

  vfs::Node *node = vfs::find(path);

  if (!node) {
    kprintf("mount: %s missing\n", path);
    return false;
  }

  node->type = vfs::VFS_MOUNT;
  node->filesystem = fs;
  node->mount_data = data;

  Mount *m = (Mount *)heap::kmalloc(sizeof(Mount));

  m->path = path;
  m->fs = fs;
  m->data = data;

  m->next = mounts;
  mounts = m;

  if (fs->init)
    return fs->init(fs);

  return true;
}

Mount *find_mount(const char *path) {

  Mount *best = nullptr;
  size_t best_len = 0;

  for (Mount *m = mounts; m; m = m->next) {

    size_t len = strlen(m->path);

    if (strncmp(path, m->path, len) == 0) {

      if (path[len] != '/' && path[len] != 0)
        continue;

      if (len > best_len) {
        best = m;
        best_len = len;
      }
    }
  }

  return best;
}

} // namespace fs
