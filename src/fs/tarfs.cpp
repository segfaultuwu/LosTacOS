#include "LTOS/fs/tarfs.hpp"

#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"

#include <string.h>

namespace fs::tarfs {

static uint8_t *tar_start = nullptr;
static size_t tar_size = 0;

static size_t octal_to_int(const char *str, size_t len) {
  size_t value = 0;

  for (size_t i = 0; i < len; i++) {
    if (str[i] == '\0' || str[i] == ' ')
      break;

    value <<= 3;
    value += str[i] - '0';
  }

  return value;
}

void mount(void *address, size_t size) {
  tar_start = (uint8_t *)address;
  tar_size = size;

  logger::info("TARFS mounted addr=%lx size=%lu", (uint64_t)address, size);
}

static bool empty_block(TarHeader *hdr) {
  for (int i = 0; i < 512; i++) {
    if (((uint8_t *)hdr)[i])
      return false;
  }

  return true;
}

File *find(const char *name) {
  if (!tar_start)
    return nullptr;

  uint8_t *ptr = tar_start;

  while (ptr < tar_start + tar_size) {
    auto *hdr = (TarHeader *)ptr;

    if (empty_block(hdr))
      break;

    size_t size = octal_to_int(hdr->size, sizeof(hdr->size));

    if (strcmp(hdr->name, name) == 0) {
      static File file;

      strncpy(file.name, hdr->name, sizeof(file.name));
      file.name[sizeof(file.name) - 1] = '\0';

      file.data = ptr + 512;

      file.size = size;

      file.directory = hdr->typeflag == '5';

      return &file;
    }

    size_t blocks = (size + 511) / 512;

    ptr += 512 + blocks * 512;
  }

  return nullptr;
}

char *normalize(char *path) {
  if (path[0] == '.' && path[1] == '/')
    return path + 2;
  return path;
}

void mount_vfs() {
  if (!tar_start)
    return;

  uint8_t *ptr = tar_start;

  while (ptr < tar_start + tar_size) {

    auto *hdr = (TarHeader *)ptr;

    if (empty_block(hdr))
      break;

    size_t size = octal_to_int(hdr->size, sizeof(hdr->size));

    char *path = hdr->name;

    path = normalize(path);

    if (path[0] == '\0') {
      ptr += 512 + ((size + 511) / 512) * 512;
      continue;
    }

    kprintf("TAR -> VFS %s\n", path);

    if (hdr->typeflag == '5') {

      fs::vfs::create_dir_path(path);

    } else {

      auto *node = fs::vfs::create_file_path(path);

      if (node) {
        node->file = (fs::vfs::File *)heap::kmalloc(sizeof(fs::vfs::File));

        node->file->private_data = ptr + 512;
        node->file->size = size;
      }
    }

    ptr += 512 + ((size + 511) / 512) * 512;
  }
}

void list() {
  if (!tar_start)
    return;

  uint8_t *ptr = tar_start;

  while (ptr < tar_start + tar_size) {
    auto *hdr = (TarHeader *)ptr;

    if (empty_block(hdr))
      break;

    kprintf("%s %s \n", hdr->typeflag == '5' ? "[DIR]" : "[FILE]", hdr->name);

    size_t size = octal_to_int(hdr->size, sizeof(hdr->size));

    ptr += 512 + ((size + 511) / 512) * 512;
  }
}

void read(File *file, void *buffer, size_t size) {
  if (!file || !buffer)
    return;

  if (size > file->size)
    size = file->size;

  memcpy(buffer, file->data, size);
}

size_t filesize(File *file) {
  if (!file)
    return 0;

  return file->size;
}

} // namespace fs::tarfs
