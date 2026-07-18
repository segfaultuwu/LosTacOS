#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fs::vfs {

struct DevOps {
  size_t (*write)(const char *buf, size_t len);
  size_t (*read)(char *buf, size_t len);
};

struct File {
  uint64_t size;
  uint64_t offset;

  void *private_data;

  int (*read)(File *file, uint8_t *buffer, size_t size);

  int (*write)(File *file, const uint8_t *buffer, size_t size);
};

enum NodeType { VFS_FILE, VFS_DIR, VFS_DEV };

struct Node {

  const char *name;

  bool directory;

  NodeType type;

  Node *parent;
  Node *children;
  Node *next;

  File *file;

  DevOps *dev;
};

extern Node *root;
extern Node *current_dir;

void init();

// Create nodes
Node *create_file(const char *name);
Node *create_dir(const char *name);
Node *create_dev(const char *name, DevOps *dev);
Node *create_node(const char *name, bool directory, Node *parent);

// Search
Node *find(const char *name);
Node *find_in(Node *dir, const char *name);

// Directory listing
void list_dir(Node *node);

// Path handling
char *get_path(Node *node);

// Current path shit idk how to explain properly
Node *get_current();
Node *set_current();

Node *create_file(const char *name);

Node *create_dir(const char *name);

char *get_name(Node *node);

char *get_content(Node *node);

void change_dir(char *path);

bool write_content(const char *path, const char *data, size_t len);
bool append_content(const char *path, const char *data, size_t len);

size_t write(Node *node, const char *buf, size_t len);

bool remove(const char *path);

} // namespace fs::vfs
