#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fs::vfs {

struct File {
  uint64_t size;
  uint64_t offset;

  void *private_data;

  int (*read)(File *file, uint8_t *buffer, size_t size);

  int (*write)(File *file, const uint8_t *buffer, size_t size);
};

struct Node {
  const char *name;

  bool directory;

  Node *parent;
  Node *next;
  Node *children;

  File *file;
};

extern Node *root;
extern Node *current_dir;

void init();

// Create nodes
Node *create_file(const char *name);
Node *create_dir(const char *name);

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

bool remove(const char *path);

} // namespace fs::vfs
