#pragma once

#include <stddef.h>

namespace fs {

struct FileSystem;

struct Mount {
  const char *path;
  FileSystem *fs;
  void *data;

  Mount *next;
};

struct FileSystem {

  const char *name;

  bool (*init)(FileSystem *fs);

  void *(*open)(void *data, const char *path);

  int (*read)(void *file, char *buf, size_t size);

  int (*write)(void *file, const char *buf, size_t size);

  void (*close)(void *file);

  void (*list)(void *data);

  int (*ioctl)(void *file, unsigned long req, void *arg);
};

bool mount(const char *path, FileSystem *fs, void *data);

Mount *find_mount(const char *path);

} // namespace fs
