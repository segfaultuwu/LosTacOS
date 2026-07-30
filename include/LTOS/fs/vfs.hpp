#pragma once

#include "LTOS/fs/fs.hpp"
#include <stddef.h>
#include <stdint.h>

namespace fs::vfs {

struct DevOps {
  size_t (*write)(const char *buf, size_t len, size_t offset);
  size_t (*read)(char *buf, size_t len, size_t offset);
  int (*ioctl)(unsigned long req, void *arg);
};

struct File {
  uint64_t size;
  uint64_t offset;

  void *private_data;

  int (*read)(File *file, uint8_t *buffer, size_t size);

  int (*write)(File *file, const uint8_t *buffer, size_t size);
};

enum NodeType { VFS_FILE, VFS_DIR, VFS_DEV, VFS_SYMLINK, VFS_MOUNT };

struct Node {

  const char *name;

  bool directory;

  NodeType type;

  Node *parent;
  Node *next;
  Node *children;

  char *symlink;

  FileSystem *filesystem;
  void *mount_data;

  File *file;
  DevOps *dev;
};

struct Dirent {
  uint64_t d_ino;
  uint64_t d_off;
  uint16_t d_reclen;
  uint8_t d_type;
  char d_name[256];
};

enum DirType {
  DT_UNKNOWN = 0,
  DT_FIFO = 1,
  DT_CHR = 2,
  DT_DIR = 4,
  DT_BLK = 6,
  DT_REG = 8,
  DT_LNK = 10,
  DT_SOCK = 12
};

extern Node *root;
extern Node *current_dir;

void init();

// Create nodes
Node *create_file(const char *name);
Node *create_dir(const char *name);
Node *create_dev(const char *name, DevOps *dev);
Node *create_node(const char *name, bool directory, Node *parent);
Node *create_file_path(const char *path);
Node *create_dir_path(const char *path);

// Search
Node *find(const char *name);
Node *find_in(Node *dir, const char *name);

// Directory listing
void list_dir(Node *node);

// Path handling
char *get_path(Node *node);

// Current path shit idk how to explain properly
Node *get_current();
void set_current(Node *node);

Node *create_file(const char *name);

Node *create_dir(const char *name);

Node *create_symlink_path(const char *path, const char *target);

char *get_name(Node *node);

char *get_content(Node *node);

void change_dir(char *path);

bool write_content(const char *path, const char *data, size_t len);
bool append_content(const char *path, const char *data, size_t len);

size_t write(Node *node, const char *buf, size_t len);

bool remove(const char *path);

} // namespace fs::vfs
