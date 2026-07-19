#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fs::tarfs {

struct TarHeader {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char typeflag;
  char linkname[100];

  char magic[6];
  char version[2];

  char uname[32];
  char gname[32];

  char devmajor[8];
  char devminor[8];

  char prefix[155];
  char padding[12];
} __attribute__((packed));

struct File {
  char name[256];

  uint8_t *data;
  size_t size;

  bool directory;
};

void mount(void *address, size_t size);
void mount_vfs();

File *find(const char *name);

void list();

void read(File *file, void *buffer, size_t size);

size_t filesize(File *file);

} // namespace fs::tarfs
