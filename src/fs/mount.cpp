#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"

#include <string.h>

namespace fs {

static Mount *mounts = nullptr;

bool mount(const char *path, FileSystem *fs, void *data) {
  if (!path || !path[0])
    return false;

  vfs::Node *node = vfs::find(path);

  if (!node) {
    node = vfs::create_dir_path(path);
  }

  if (!node) {
    kprintf("mount: %s missing\n", path);
    return false;
  }

  node->type = vfs::VFS_MOUNT;
  node->filesystem = fs;
  node->mount_data = data;

  Mount *m = (Mount *)heap::kmalloc(sizeof(Mount));
  if (!m)
    return false;

  size_t path_len = strlen(path);
  m->path = (char *)heap::kmalloc(path_len + 1);
  if (m->path) {
    strcpy(m->path, path);
  }

  m->fs = fs;
  m->data = data;

  m->next = mounts;
  mounts = m;

  if (fs && fs->init)
    return fs->init(fs);

  return true;
}

bool umount(const char *path) {
  if (!path || !mounts)
    return false;

  Mount **pp = &mounts;
  while (*pp) {
    if ((*pp)->path && strcmp((*pp)->path, path) == 0) {
      Mount *m = *pp;
      *pp = m->next;

      vfs::Node *node = vfs::find(path);
      if (node && node->type == vfs::VFS_MOUNT) {
        node->type = vfs::VFS_DIR;
        node->filesystem = nullptr;
        node->mount_data = nullptr;
      }

      if (m->path) {
        heap::kfree(m->path);
      }
      heap::kfree(m);
      return true;
    }
    pp = &(*pp)->next;
  }

  return false;
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

Mount *get_mounts() {
  return mounts;
}

} // namespace fs
