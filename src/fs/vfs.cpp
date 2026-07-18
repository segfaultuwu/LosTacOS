#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"

#include <stddef.h>
#include <stdint.h>

namespace fs::vfs {

Node *root = nullptr;
Node *current_dir = nullptr;

static char *strdup(const char *s) {
  size_t len = 0;

  while (s[len])
    len++;

  char *out = (char *)heap::kmalloc(len + 1);

  if (!out)
    return nullptr;

  for (size_t i = 0; i < len; i++)
    out[i] = s[i];

  out[len] = 0;

  return out;
}

void init() {
  root = (Node *)heap::kmalloc(sizeof(Node));

  root->name = strdup("/");
  root->directory = true;

  root->parent = nullptr;

  root->next = nullptr;
  root->children = nullptr;
  root->file = nullptr;

  current_dir = root;

  // Create the basic directories
  create_dir("root");
  create_dir("tmp");
  create_dir("dev");
  create_dir("proc");
  create_dir("sys");
  create_dir("home");
  create_dir("bin");
  create_dir("etc");
  create_dir("usr");
  create_dir("var");
  create_dir("lib");
  create_dir("mnt");
}

static Node *create_node(const char *name, bool directory) {
  Node *node = (Node *)heap::kmalloc(sizeof(Node));

  node->name = strdup(name);
  node->directory = directory;

  node->parent = current_dir;

  node->children = nullptr;
  node->file = nullptr;

  node->next = current_dir->children;
  current_dir->children = node;

  return node;
}

Node *create_file(const char *name) { return create_node(name, false); }

Node *create_dir(const char *name) { return create_node(name, true); }

char *get_name(Node *node) { return (char *)node->name; }

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

void set_current(Node *node) {
  if (node && node->directory)
    current_dir = node;
}

Node *get_current() { return current_dir; }

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

  // absolutna czy relatywna
  if (path[0] == '/')
    node = root;
  else
    node = current_dir;

  char part[128];
  size_t i = 0;

  for (size_t p = (path[0] == '/' ? 1 : 0);; p++) {
    if (path[p] == '/' || path[p] == 0) {
      part[i] = 0;

      if (i == 0) {
        // skip //
      } else if (part[0] == '.' && part[1] == 0) {
        // .
      } else if (part[0] == '.' && part[1] == '.' && part[2] == 0) {
        if (node->parent)
          node = node->parent;
      } else {
        node = find_in(node, part);
        if (!node)
          return nullptr;
      }

      i = 0;

      if (path[p] == 0)
        break;
    } else {
      part[i++] = path[p];
    }
  }

  return node;
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
    char segment[128];
    size_t slen = 0;

    while (node->name[slen])
      slen++;

    // copy name
    for (size_t i = 0; i < slen; i++)
      segment[i] = node->name[i];

    segment[slen] = 0;

    // prepend: "/name"
    char newtemp[1024];
    newtemp[0] = '/';

    size_t i = 1;

    for (size_t j = 0; j < slen; j++)
      newtemp[i++] = segment[j];

    for (size_t j = 0; temp[j]; j++)
      newtemp[i++] = temp[j];

    newtemp[i] = 0;

    // copy back
    for (size_t j = 0; j <= i; j++)
      temp[j] = newtemp[j];

    node = node->parent;
  }

  for (size_t i = 0; temp[i]; i++)
    path[i] = temp[i];

  path[len = 0];
  while (temp[len]) {
    path[len] = temp[len];
    len++;
  }

  path[len] = 0;

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

} // namespace fs::vfs
