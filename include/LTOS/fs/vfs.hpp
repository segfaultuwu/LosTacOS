#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fs::vfs {

struct File;

struct Node {
  const char *name;

  bool directory;

  Node *parent;
  Node *next;
  Node *children;

  File *file;
};

struct File {
  uint64_t size;
  uint64_t offset;

  void *private_data;

  int (*read)(File *file, uint8_t *buffer, size_t size);

  int (*write)(File *file, const uint8_t *buffer, size_t size);
};

extern Node *root;
extern Node *current_dir;

void init();

// Create nodes
Node *create_file(const char *name);
Node *create_dir(const char *name);

// Search
Node *find(const char *name);

// Directory listing
void list_dir();

// Path handling
char *get_path(Node *node);

// Current path shit idk how to explain properly
Node *get_current();
Node *set_current();

} // namespace fs::vfs
