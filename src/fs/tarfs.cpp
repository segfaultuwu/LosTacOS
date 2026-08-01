#include "LTOS/fs/tarfs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/gzip.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"

#include <string.h>

namespace fs::tarfs {

struct TarEntry {
  char name[256];
  uint8_t *data;
  size_t size;
  bool directory;
};

constexpr size_t MAX_TAR_FILES = 512;

static TarEntry entries[MAX_TAR_FILES];
static size_t entry_count = 0;

static uint8_t *tar_start = nullptr;
static size_t tar_size = 0;

static bool empty_block(TarHeader *hdr) {
  for (int i = 0; i < 512; i++) {
    if (((uint8_t *)hdr)[i])
      return false;
  }

  return true;
}

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

static void build_index() {
  uint8_t *ptr = tar_start;

  entry_count = 0;

  while (ptr < tar_start + tar_size) {
    auto *hdr = (TarHeader *)ptr;

    if (empty_block(hdr))
      break;

    size_t size = octal_to_int(hdr->size, sizeof(hdr->size));

    if (entry_count < MAX_TAR_FILES) {
      TarEntry *e = &entries[entry_count++];

      strncpy(e->name, hdr->name, sizeof(e->name) - 1);

      e->name[sizeof(e->name) - 1] = 0;

      e->data = ptr + 512;
      e->size = size;
      e->directory = hdr->typeflag == '5';
    }

    ptr += 512 + ((size + 511) / 512) * 512;
  }

  logger::info("tarfs indexed %lu files", entry_count);
}

void mount(void *address, size_t size) {
  if (::gzip::is_gzip(address, size)) {
    logger::info("tarfs: detected GZIP compressed rootfs archive");
    size_t out_size = 4 * 1024 * 1024; // 4MB buffer for uncompressed rootfs tar
    void *uncompressed = heap::kmalloc(out_size);
    if (uncompressed && ::gzip::decompress(address, size, uncompressed, &out_size)) {
      address = uncompressed;
      size = out_size;
    }
  }

  tar_start = (uint8_t *)address;
  tar_size = size;

  build_index();
}

File *find(const char *name) {
  for (size_t i = 0; i < entry_count; i++) {
    if (strcmp(entries[i].name, name) == 0) {
      static File file;

      strncpy(file.name, entries[i].name, sizeof(file.name) - 1);

      file.name[sizeof(file.name) - 1] = '\0';

      file.data = entries[i].data;
      file.size = entries[i].size;
      file.directory = entries[i].directory;

      return &file;
    }
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
    size_t blocks = (size + 511) / 512;

    char path_buf[300];

    if (hdr->prefix[0] != '\0') {
      char prefix[156];
      char name[101];

      strncpy(prefix, hdr->prefix, sizeof(hdr->prefix));
      prefix[sizeof(hdr->prefix)] = '\0';

      strncpy(name, hdr->name, sizeof(hdr->name));
      name[sizeof(hdr->name)] = '\0';

      size_t plen = strlen(prefix);
      strncpy(path_buf, prefix, plen);
      path_buf[plen] = '/';
      strncpy(path_buf + plen + 1, name, strlen(name) + 1);
    } else {
      strncpy(path_buf, hdr->name, sizeof(hdr->name));
      path_buf[sizeof(hdr->name)] = '\0';
    }

    char *path = normalize(path_buf);

    size_t plen = strlen(path);
    if (plen > 0 && path[plen - 1] == '/')
      path[plen - 1] = '\0';

    if (hdr->typeflag == '2') {
      char link_buf[101];

      strncpy(link_buf, hdr->linkname, sizeof(hdr->linkname));

      link_buf[sizeof(hdr->linkname)] = '\0';

      auto node = fs::vfs::create_symlink_path(path, link_buf);

      if (!node) {
        logger::warn("tarfs: failed symlink %s", path);
      }

      ptr += 512 + blocks * 512;
      continue;
    }

    bool is_dir = hdr->typeflag == '5';

    if (path[0] == '\0') {
      ptr += 512 + blocks * 512;
      continue;
    }

    if (is_dir) {
      fs::vfs::create_dir_path(path);
    } else if (hdr->typeflag == '0' || hdr->typeflag == '\0') {
      auto node = fs::vfs::create_file_path(path);

      if (node) {
        node->file = (fs::vfs::File *)heap::kmalloc(sizeof(fs::vfs::File));

        node->file->private_data = ptr + 512;
        node->file->size = size;
        node->file->read = nullptr;
        node->file->write = nullptr;
      } else {
        logger::warn("tarfs: failed to add %s", path);
      }
    }

    ptr += 512 + blocks * 512;
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
