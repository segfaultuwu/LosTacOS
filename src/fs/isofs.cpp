#include "LTOS/fs/isofs.hpp"
#include "LTOS/drivers/ahci.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include <string.h>

namespace fs::isofs {

struct IsoPVD {
  uint8_t type;
  char id[5];
  uint8_t version;
  uint8_t unused1;
  char system_id[32];
  char volume_id[32];
  uint8_t unused2[8];
  uint32_t volume_space_size_le;
  uint32_t volume_space_size_be;
  uint8_t unused3[32];
  uint16_t volume_set_size_le;
  uint16_t volume_set_size_be;
  uint16_t volume_seq_num_le;
  uint16_t volume_seq_num_be;
  uint16_t logical_block_size_le;
  uint16_t logical_block_size_be;
  uint32_t path_table_size_le;
  uint32_t path_table_size_be;
  uint32_t path_table_le;
  uint32_t path_table_opt_le;
  uint32_t path_table_be;
  uint32_t path_table_opt_be;
  IsoDirectoryRecord root_dir;
} __attribute__((packed));

struct IsoVolume {
  uint8_t port;
  uint32_t block_size;
  uint32_t root_extent;
  uint32_t root_size;
};

struct IsoHandle {
  IsoVolume *vol;
  uint32_t extent;
  uint32_t size;
  size_t offset;
  bool is_dir;
};

static bool read_block(uint8_t port, uint32_t lba, void *buf) {
  return drivers::ahci::atapi::read(port, lba, 1, buf);
}

static void *isofs_open(void *data, const char *path) {
  IsoVolume *vol = (IsoVolume *)data;
  if (!vol || !path)
    return nullptr;

  while (*path == '/')
    path++;

  if (*path == 0) {
    IsoHandle *h = (IsoHandle *)heap::kmalloc(sizeof(IsoHandle));
    if (!h)
      return nullptr;
    h->vol = vol;
    h->extent = vol->root_extent;
    h->size = vol->root_size;
    h->offset = 0;
    h->is_dir = true;
    return h;
  }

  uint8_t block[2048];
  if (!read_block(vol->port, vol->root_extent, block))
    return nullptr;

  uint32_t dir_extent = vol->root_extent;
  uint32_t dir_size = vol->root_size;

  char component[256];
  const char *p = path;

  while (*p) {
    size_t i = 0;
    while (*p && *p != '/') {
      if (i < sizeof(component) - 1)
        component[i++] = *p;
      p++;
    }
    component[i] = 0;
    while (*p == '/')
      p++;

    if (i == 0)
      continue;

    bool found = false;
    uint8_t *dir_data = (uint8_t *)heap::kmalloc(dir_size + 2048);
    if (!dir_data)
      return nullptr;

    uint32_t remaining = dir_size;
    uint32_t curr_extent = dir_extent;

    while (remaining > 0) {
      uint32_t chunk = remaining < 2048 ? remaining : 2048;
      if (!read_block(vol->port, curr_extent, dir_data + (dir_size - remaining)))
        break;

      remaining -= chunk;
      curr_extent++;
    }

    uint8_t *ptr = dir_data;
    uint8_t *end = dir_data + dir_size;

    while (ptr < end) {
      IsoDirectoryRecord *rec = (IsoDirectoryRecord *)ptr;
      if (rec->length == 0) {
        ptr++;
        continue;
      }

      if (ptr + rec->length > end)
        break;

      size_t name_len = rec->name_len;
      char rec_name[256];

      if (name_len >= 2 && rec->name[0] == 0) {
        name_len = 1;
        rec_name[0] = '.';
        rec_name[1] = 0;
      } else if (name_len >= 2 && rec->name[0] == 1) {
        name_len = 2;
        rec_name[0] = '.';
        rec_name[1] = '.';
        rec_name[2] = 0;
      } else {
        bool had_semicolon = false;
        size_t ri = 0;
        for (size_t j = 0; j < name_len; j++) {
          if (rec->name[j] == ';') {
            had_semicolon = true;
            break;
          }
          rec_name[ri++] = rec->name[j];
        }
        rec_name[ri] = 0;
        name_len = ri;

        if (!had_semicolon) {
          for (size_t j = name_len; j > 0; j--) {
            if (rec_name[j - 1] == '.') {
              if (j >= 3 && rec_name[j - 2] == ';' && rec_name[j - 3] == '1')
                name_len = j - 3;
              break;
            }
          }
        }

        for (size_t j = 0; j < name_len; j++) {
          if (rec_name[j] >= 'A' && rec_name[j] <= 'Z')
            rec_name[j] += 32;
        }
      }

      if (name_len == i && strncmp(rec_name, component, i) == 0) {
        dir_extent = rec->extent_lba_le;
        dir_size = rec->data_length_le;

        if (*p == 0) {
          heap::kfree(dir_data);

          IsoHandle *h = (IsoHandle *)heap::kmalloc(sizeof(IsoHandle));
          if (!h)
            return nullptr;

          h->vol = vol;
          h->extent = rec->extent_lba_le;
          h->size = rec->data_length_le;
          h->offset = 0;
          h->is_dir = (rec->flags & ISO_FLAG_DIRECTORY) != 0;
          return h;
        }

        found = true;
        break;
      }

      ptr += rec->length;
    }

    heap::kfree(dir_data);

    if (!found)
      return nullptr;

  }

  return nullptr;
}

static int isofs_read(void *file, char *buf, size_t size) {
  IsoHandle *h = (IsoHandle *)file;
  if (!h || h->is_dir)
    return -1;

  if (h->offset >= h->size)
    return 0;

  size_t remaining = h->size - h->offset;
  size_t to_read = size < remaining ? size : remaining;
  size_t read_bytes = 0;

  uint8_t block[2048];

  while (read_bytes < to_read) {
    uint32_t lba = h->extent + (h->offset / h->vol->block_size);
    uint32_t block_off = h->offset % h->vol->block_size;

    if (!read_block(h->vol->port, lba, block))
      break;

    size_t chunk = h->vol->block_size - block_off;
    if (chunk > to_read - read_bytes)
      chunk = to_read - read_bytes;

    memcpy(buf + read_bytes, block + block_off, chunk);
    read_bytes += chunk;
    h->offset += chunk;
  }

  return (int)read_bytes;
}

static int isofs_write(void *file, const char *buf, size_t size) {
  (void)file;
  (void)buf;
  (void)size;
  return -1;
}

static void isofs_close(void *file) {
  if (file)
    heap::kfree(file);
}

static void isofs_list(void *data) {
  IsoVolume *vol = (IsoVolume *)data;
  if (!vol)
    return;

  uint8_t *dir_data = (uint8_t *)heap::kmalloc(vol->root_size + 2048);
  if (!dir_data)
    return;

  uint32_t remaining = vol->root_size;
  uint32_t curr_extent = vol->root_extent;

  while (remaining > 0) {
    uint32_t chunk = remaining < 2048 ? remaining : 2048;
    if (!read_block(vol->port, curr_extent, dir_data + (vol->root_size - remaining)))
      break;
    remaining -= chunk;
    curr_extent++;
  }

  uint8_t *ptr = dir_data;
  uint8_t *end = dir_data + vol->root_size;

  while (ptr < end) {
    IsoDirectoryRecord *rec = (IsoDirectoryRecord *)ptr;
    if (rec->length == 0) {
      ptr++;
      continue;
    }

    if (ptr + rec->length > end)
      break;

    if (rec->name_len == 1 && (rec->name[0] == 0 || rec->name[0] == 1)) {
      ptr += rec->length;
      continue;
    }

    char name[256];
    size_t name_len = rec->name_len;
    size_t ri = 0;

    for (size_t j = 0; j < name_len; j++) {
      if (rec->name[j] == ';')
        break;
      name[ri++] = rec->name[j];
    }
    name[ri] = 0;

    for (size_t j = 0; j < ri; j++) {
      if (name[j] >= 'A' && name[j] <= 'Z')
        name[j] += 32;
      if (name[j] == '.') {
        bool only_version = true;
        for (size_t k = j + 1; k < ri; k++) {
          if (name[k] < '0' || name[k] > '9') {
            only_version = false;
            break;
          }
        }
        if (only_version)
          name[j] = 0;
      }
    }

    kprintf("%s%s\n", name, (rec->flags & ISO_FLAG_DIRECTORY) ? "/" : "");
    ptr += rec->length;
  }

  heap::kfree(dir_data);
}

static bool isofs_init(FileSystem *fs) {
  (void)fs;
  return true;
}

FileSystem filesystem = {.name = "isofs",
                         .init = isofs_init,
                         .open = isofs_open,
                         .read = isofs_read,
                         .write = isofs_write,
                         .close = isofs_close,
                         .list = isofs_list,
                         .ioctl = nullptr};

void *mount(uint8_t atapi_port) {
  uint8_t block[2048];
  if (!read_block(atapi_port, 16, block)) {
    drivers::serial::writef("ISOFS: failed to read PVD at LBA 16\n");
    return nullptr;
  }

  IsoPVD *pvd = (IsoPVD *)block;
  if (pvd->type != 1 || strncmp(pvd->id, "CD001", 5) != 0) {
    drivers::serial::writef("ISOFS: not a valid ISO9660 PVD (type=%d id=%.5s)\n", pvd->type,
                            pvd->id);
    return nullptr;
  }

  IsoVolume *vol = (IsoVolume *)heap::kmalloc(sizeof(IsoVolume));
  if (!vol)
    return nullptr;

  vol->port = atapi_port;
  vol->block_size = pvd->logical_block_size_le;
  vol->root_extent = pvd->root_dir.extent_lba_le;
  vol->root_size = pvd->root_dir.data_length_le;

  drivers::serial::writef("ISOFS: mounted port %d blocksize=%u root_extent=%u root_size=%u\n",
                          atapi_port, vol->block_size, vol->root_extent, vol->root_size);

  return vol;
}

} // namespace fs::isofs
