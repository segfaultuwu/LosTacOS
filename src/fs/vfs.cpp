#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS/sched/task.hpp"
#include "LTOS/state.hpp"

#include <stddef.h>
#include <string.h>

namespace fs::vfs {

Node *root = nullptr;
Node *current_dir = nullptr;

void init() {
  root = (Node *)heap::kmalloc(sizeof(Node));

  root->filesystem = nullptr;
  root->mount_data = nullptr;

  root->name = strdup("/");
  root->directory = true;
  root->type = VFS_DIR;

  root->parent = nullptr;

  root->next = nullptr;
  root->children = nullptr;
  root->file = nullptr;

  current_dir = root;
  state::vfs_initialized = true;
}

Node *create_node(const char *name, bool directory, Node *parent) {
  Node *node = (Node *)heap::kmalloc(sizeof(Node));

  node->name = strdup(name);
  node->directory = directory;
  node->type = directory ? VFS_DIR : VFS_FILE;

  node->parent = parent;

  node->children = nullptr;
  node->file = nullptr;
  node->dev = nullptr;

  node->filesystem = nullptr;
  node->mount_data = nullptr;

  node->next = parent->children;
  parent->children = node;

  return node;
}

void mount_node(Node *node, fs::FileSystem *fs, void *data) {
  node->type = VFS_MOUNT;
  node->filesystem = fs;
  node->mount_data = data;
}

Node *create_dev(const char *name, DevOps *dev) {
  Node *dev_dir = find("/dev");

  if (!dev_dir) {
    kprintf("devfs: /dev missing\n");
    return nullptr;
  }

  Node *node = create_node(name, false, dev_dir);

  node->dev = dev;
  node->type = VFS_DEV;

  return node;
}

size_t write(Node *node, const char *buf, size_t len) {
  if (!node)
    return 0;

  if (node->dev) {
    node->dev->write(buf, len, 0);
    return len;
  }

  return 0;
}

Node *create_file(const char *name) {
  return create_node(name, false, current_dir);
}

Node *create_dir(const char *name) {
  return create_node(name, true, current_dir);
}

static Node *ensure_dir(Node *parent, const char *name) {
  Node *node = find_in(parent, name);

  if (node)
    return node;

  return create_node(name, true, parent);
}

Node *create_file_path(const char *path) {
  if (!path || !root || path[0] == '\0')
    return nullptr;

  Node *dir = root;

  const char *p = path;

  while (*p == '/')
    p++;

  char part[128];

  while (*p) {

    size_t i = 0;

    while (*p && *p != '/') {
      if (i < sizeof(part) - 1)
        part[i++] = *p;

      p++;
    }

    part[i] = 0;

    while (*p == '/')
      p++;

    if (i == 0)
      continue;

    if (*p == 0) {

      Node *existing = find_in(dir, part);

      if (existing)
        return existing;

      return create_node(part, false, dir);
    }

    Node *next = find_in(dir, part);

    if (!next) {
      next = create_node(part, true, dir);
    } else if (!next->directory) {
      kprintf("VFS: %s is not directory\n", part);
      return nullptr;
    }

    dir = next;
  }

  return nullptr;
}

Node *create_dir_path(const char *path) {
  Node *dir = root;

  const char *p = path;
  char part[128];

  while (*p) {

    size_t i = 0;

    while (*p && *p != '/') {
      part[i++] = *p++;
    }

    part[i] = 0;

    if (*p)
      p++;

    Node *next = find_in(dir, part);

    if (!next)
      next = create_node(part, true, dir);

    dir = next;
  }

  return dir;
}

char *get_name(Node *node) {
  return (char *)node->name;
}

char *get_content(Node *node) {
  if (node->file)
    return (char *)node->file->private_data;

  return nullptr;
}

void list_dir(Node *n) {
  if (!n)
    n = current_dir;

  Node *node = n->children;

  while (node) {
    if (node->directory)
      kprintf("\033[34m[DIR ]\033[0m %s\n", node->name);
    else
      kprintf("\033[32m[FILE]\033[0m %s\n", node->name);

    node = node->next;
  }
}

Node *create_symlink_path(const char *path, const char *target) {
  Node *node = create_file_path(path);

  if (!node)
    return nullptr;

  node->type = NodeType::VFS_SYMLINK;

  node->symlink = (char *)heap::kmalloc(strlen(target) + 1);

  strcpy(node->symlink, target);

  return node;
}

void set_current(Node *node) {
  if (node && node->directory)
    current_dir = node;
}

Node *get_current() {
  return current_dir;
}

void change_dir(char *path) {
  Node *node = find(path);

  if (!node) {
    kprintf("cd: no such directory\n");
    return;
  }

  if (!node->directory) {
    kprintf("cd: not a directory\n");
    return;
  }

  current_dir = node;
}

Node *find(const char *path) {
  if (!path || !path[0])
    return current_dir;

  Node *node;

  if (path[0] == '/')
    node = root;
  else {
    sched::Task *task = sched::get_current();

    if (task && task->process && task->process->cwd)
      node = task->process->cwd;
    else
      node = root;
  }

  char part[128];
  size_t i = 0;

  for (size_t p = (path[0] == '/' ? 1 : 0);; p++) {

    if (path[p] == '/' || path[p] == 0) {

      part[i] = 0;

      if (i == 0) {

      } else if (part[0] == '.' && part[1] == 0) {

      } else if (part[0] == '.' && part[1] == '.' && part[2] == 0) {

        if (node->parent)
          node = node->parent;

      } else {

        node = find_in(node, part);

        if (!node)
          return nullptr;

        if (node->type == VFS_MOUNT) {
          return node;
        }

        // resolve symlinks
        int depth = 0;

        while (node->type == NodeType::VFS_SYMLINK) {

          if (depth++ >= 8)
            return nullptr;

          char target[256];

          if (node->symlink[0] == '/') {

            strcpy(target, node->symlink);

          } else {

            // resolve relative to the symlink's parent *directory*, not
            // just its immediate name -- node->parent->name is only one
            // path component, so this used to break for any symlink
            // nested more than one level deep.
            const char *parent_path = node->parent ? get_path(node->parent) : "/";

            strcpy(target, parent_path);

            size_t tlen = strlen(target);
            if (tlen == 0 || target[tlen - 1] != '/')
              strcat(target, "/");

            strcat(target, node->symlink);
          }

          node = find(target);

          if (!node)
            return nullptr;
        }
      }

      i = 0;

      if (path[p] == 0)
        break;

    } else {

      if (i < sizeof(part) - 1)
        part[i++] = path[p];
    }
  }

  return node;
}

Node *get_dev_dir() {
  return find("/dev");
}

Node *find_in(Node *dir, const char *name) {
  Node *node = dir->children;

  while (node) {
    const char *a = node->name;
    const char *b = name;

    bool same = true;

    while (*a && *b) {
      if (*a != *b) {
        same = false;
        break;
      }
      a++;
      b++;
    }

    if (same && *a == 0 && *b == 0)
      return node;

    node = node->next;
  }

  return nullptr;
}

Node *get_process_cwd() {
  sched::Task *task = sched::get_current();

  if (task && task->process && task->process->cwd)
    return task->process->cwd;

  return root;
}

char *get_path(Node *node) {
  static char path[1024];
  char temp[1024];

  size_t len = 0;

  if (node == root) {
    path[0] = '/';
    path[1] = 0;
    return path;
  }

  temp[0] = 0;

  while (node && node != root) {
    char newtemp[1024];

    size_t slen = strlen(node->name);

    newtemp[0] = '/';

    for (size_t i = 0; i < slen; i++)
      newtemp[i + 1] = node->name[i];

    size_t pos = slen + 1;

    for (size_t i = 0; temp[i]; i++)
      newtemp[pos++] = temp[i];

    newtemp[pos] = 0;

    strcpy(temp, newtemp);

    node = node->parent;
  }

  strcpy(path, temp);

  return path;
}

static bool ensure_file_storage(Node *node) {
  if (node->directory)
    return false;

  if (!node->file) {
    node->file = (File *)heap::kmalloc(sizeof(File));
    if (!node->file)
      return false;

    node->file->private_data = nullptr;
    node->file->size = 0;
  }

  return true;
}

bool write_content(const char *path, const char *data, size_t len) {
  Node *node = find(path);

  if (!node)
    node = create_file(path);

  if (!node) {
    kprintf("write: could not create %s\n", path);
    return false;
  }

  if (node->directory) {
    kprintf("write: %s is a directory\n", path);
    return false;
  }

  if (!ensure_file_storage(node))
    return false;

  char *buf = (char *)heap::kmalloc(len + 1);
  if (!buf)
    return false;

  for (size_t i = 0; i < len; i++)
    buf[i] = data[i];
  buf[len] = 0;

  if (node->file->private_data)
    heap::kfree(node->file->private_data);

  node->file->private_data = buf;
  node->file->size = len;

  return true;
}

bool append_content(const char *path, const char *data, size_t len) {
  Node *node = find(path);

  if (!node)
    return write_content(path, data, len);

  if (node->directory) {
    kprintf("append: %s is a directory\n", path);
    return false;
  }

  if (!ensure_file_storage(node))
    return false;

  size_t old_len = node->file->size;
  char *old_data = (char *)node->file->private_data;

  size_t new_len = old_len + len;
  char *buf = (char *)heap::kmalloc(new_len + 1);
  if (!buf)
    return false;

  for (size_t i = 0; i < old_len; i++)
    buf[i] = old_data[i];

  for (size_t i = 0; i < len; i++)
    buf[old_len + i] = data[i];

  buf[new_len] = 0;

  if (old_data)
    heap::kfree(old_data);

  node->file->private_data = buf;
  node->file->size = new_len;

  return true;
}

bool remove(const char *path) {
  Node *node = find(path);

  if (!node) {
    kprintf("rm: %s: no such file or directory\n", path);
    return false;
  }

  if (node == root) {
    kprintf("rm: cannot remove root\n");
    return false;
  }

  if (node->directory && node->children) {
    kprintf("rm: %s: directory not empty\n", path);
    return false;
  }

  Node *parent = node->parent;

  if (parent->children == node) {
    parent->children = node->next;
  } else {
    Node *prev = parent->children;
    while (prev && prev->next != node)
      prev = prev->next;

    if (prev)
      prev->next = node->next;
  }

  if (!node->directory && node->file) {
    if (node->file->private_data)
      heap::kfree(node->file->private_data);
    heap::kfree(node->file);
  }

  heap::kfree((void *)node->name);
  heap::kfree(node);

  return true;
}

int read(Node *node, char *buf, int size) {
  if (!node)
    return -1;

  if (node->type == VFS_MOUNT) {
    return -1;
  }

  if (node->type == VFS_DEV && node->dev && node->dev->read)
    return node->dev->read(buf, size, 0);

  if (!node->directory && node->file && node->file->private_data) {
    int len = node->file->size < (size_t)size ? node->file->size : size;

    for (int i = 0; i < len; i++)
      buf[i] = ((char *)node->file->private_data)[i];

    return len;
  }

  return -1;
}

int write(Node *node, const char *buf, int size) {
  if (!node)
    return -1;

  if (node->type == VFS_DEV && node->dev && node->dev->write)
    return node->dev->write(buf, size, 0);

  if (!node->directory)
    return write_content(node->name, buf, size) ? size : -1;

  return -1;
}

} // namespace fs::vfs
