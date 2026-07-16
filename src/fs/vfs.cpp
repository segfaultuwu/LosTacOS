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

  node->parent = root;

  node->next = root->children;

  node->children = nullptr;

  node->file = nullptr;

  root->children = node;

  return node;
}

Node *create_file(const char *name) { return create_node(name, false); }

Node *create_dir(const char *name) { return create_node(name, true); }

void list_dir() {
  Node *node = root->children;

  while (node) {
    if (node->directory)
      kprintf("[DIR ] %s\n", node->name);
    else
      kprintf("[FILE] %s\n", node->name);

    node = node->next;
  }
}

void set_current(Node *node) {
  if (node && node->directory)
    current_dir = node;
}

Node *get_current() { return current_dir; }

Node *find(const char *name) {
  Node *node = root->children;

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

  if (node == root) {
    path[0] = '/';
    path[1] = 0;

    return path;
  }

  path[0] = '/';

  size_t i = 1;

  const char *name = node->name;

  while (name[i - 1]) {
    path[i] = name[i - 1];
    i++;
  }

  path[i] = 0;

  return path;
}

} // namespace fs::vfs
