#include "LTOS/fs/fat32.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include <string.h>

extern "C" char *strchr(const char *s, int c);

namespace fs::fat32 {

struct Fat32Volume {
  uint8_t *disk_image;
  size_t disk_size;

  BPB32 bpb;
  uint32_t bytes_per_cluster;
  uint32_t fat_start_sector;
  uint32_t data_start_sector;
  uint32_t root_cluster;
};

struct Fat32FileHandle {
  Fat32Volume *vol;
  uint32_t first_cluster;
  uint32_t curr_cluster;
  uint32_t file_size;
  size_t offset;
  bool is_dir;

  // Pointer to parent directory entry on disk for file_size updates
  DirectoryEntry *dir_entry;
};

static Fat32Volume *main_vol = nullptr;

static uint8_t *get_sector_ptr(Fat32Volume *vol, uint32_t sector) {
  uint64_t offset = (uint64_t)sector * vol->bpb.bytes_per_sector;
  if (offset >= vol->disk_size)
    return nullptr;
  return vol->disk_image + offset;
}

static uint8_t *get_cluster_ptr(Fat32Volume *vol, uint32_t cluster) {
  if (cluster < 2)
    return nullptr;
  uint32_t sector = vol->data_start_sector + (cluster - 2) * vol->bpb.sectors_per_cluster;
  return get_sector_ptr(vol, sector);
}

static uint32_t read_fat(Fat32Volume *vol, uint32_t cluster) {
  uint32_t fat_offset = cluster * 4;
  uint32_t fat_sector = vol->fat_start_sector + (fat_offset / vol->bpb.bytes_per_sector);
  uint32_t entry_offset = fat_offset % vol->bpb.bytes_per_sector;

  uint8_t *sec = get_sector_ptr(vol, fat_sector);
  if (!sec)
    return 0x0FFFFFFF;

  uint32_t val = *(uint32_t *)(sec + entry_offset);
  return val & 0x0FFFFFFF;
}

static void write_fat(Fat32Volume *vol, uint32_t cluster, uint32_t value) {
  uint32_t fat_offset = cluster * 4;
  for (int f = 0; f < vol->bpb.num_fats; f++) {
    uint32_t fat_sector = vol->fat_start_sector + (f * vol->bpb.fat_size_32) +
                          (fat_offset / vol->bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % vol->bpb.bytes_per_sector;

    uint8_t *sec = get_sector_ptr(vol, fat_sector);
    if (sec) {
      uint32_t existing = *(uint32_t *)(sec + entry_offset);
      *(uint32_t *)(sec + entry_offset) = (existing & 0xF0000000) | (value & 0x0FFFFFFF);
    }
  }
}

static uint32_t alloc_cluster(Fat32Volume *vol) {
  uint32_t total_clusters =
      (vol->bpb.total_sectors_32 - vol->data_start_sector) / vol->bpb.sectors_per_cluster;
  for (uint32_t c = 2; c < total_clusters; c++) {
    if (read_fat(vol, c) == 0) {
      write_fat(vol, c, 0x0FFFFFFF);
      uint8_t *ptr = get_cluster_ptr(vol, c);
      if (ptr)
        memset(ptr, 0, vol->bytes_per_cluster);
      return c;
    }
  }
  return 0;
}

static void filename_to_short(const char *name, char short_name[11]) {
  memset(short_name, ' ', 11);
  const char *dot = strchr(name, '.');

  size_t base_len = dot ? (size_t)(dot - name) : strlen(name);
  if (base_len > 8)
    base_len = 8;

  for (size_t i = 0; i < base_len; i++) {
    char c = name[i];
    if (c >= 'a' && c <= 'z')
      c -= 32;
    short_name[i] = c;
  }

  if (dot) {
    const char *ext = dot + 1;
    size_t ext_len = strlen(ext);
    if (ext_len > 3)
      ext_len = 3;
    for (size_t i = 0; i < ext_len; i++) {
      char c = ext[i];
      if (c >= 'a' && c <= 'z')
        c -= 32;
      short_name[8 + i] = c;
    }
  }
}

static DirectoryEntry *find_entry_in_dir(Fat32Volume *vol, uint32_t dir_cluster, const char *name) {
  char target_short[11];
  filename_to_short(name, target_short);

  uint32_t curr = dir_cluster;
  while (curr >= 2 && curr < 0x0FFFFFF8) {
    uint8_t *cluster_data = get_cluster_ptr(vol, curr);
    if (!cluster_data)
      break;

    DirectoryEntry *entries = (DirectoryEntry *)cluster_data;
    size_t num_entries = vol->bytes_per_cluster / sizeof(DirectoryEntry);

    for (size_t i = 0; i < num_entries; i++) {
      if ((uint8_t)entries[i].name[0] == 0x00)
        return nullptr;
      if ((uint8_t)entries[i].name[0] == 0xE5)
        continue;
      if (entries[i].attr == ATTR_LONG_NAME)
        continue;

      if (memcmp(entries[i].name, target_short, 11) == 0) {
        return &entries[i];
      }
    }

    curr = read_fat(vol, curr);
  }

  return nullptr;
}

static DirectoryEntry *find_path(Fat32Volume *vol, const char *path) {
  if (!path || !*path || strcmp(path, "/") == 0)
    return nullptr;

  while (*path == '/')
    path++;

  uint32_t curr_cluster = vol->root_cluster;
  char component[256];
  DirectoryEntry *entry = nullptr;

  while (*path) {
    const char *next_slash = strchr(path, '/');
    size_t len = next_slash ? (size_t)(next_slash - path) : strlen(path);
    if (len >= sizeof(component))
      len = sizeof(component) - 1;

    memcpy(component, path, len);
    component[len] = '\0';

    entry = find_entry_in_dir(vol, curr_cluster, component);
    if (!entry)
      return nullptr;

    if (next_slash) {
      if (!(entry->attr & ATTR_DIRECTORY))
        return nullptr;
      curr_cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
      path = next_slash + 1;
      while (*path == '/')
        path++;
    } else {
      break;
    }
  }

  return entry;
}

static void *fat32_open(void *vol_ptr, const char *path) {
  Fat32Volume *vol = (Fat32Volume *)vol_ptr;
  if (!vol)
    vol = main_vol;
  if (!vol)
    return nullptr;

  DirectoryEntry *entry = find_path(vol, path);
  if (!entry)
    return nullptr;

  Fat32FileHandle *h = (Fat32FileHandle *)heap::kmalloc(sizeof(Fat32FileHandle));
  if (!h)
    return nullptr;

  h->vol = vol;
  h->first_cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
  h->curr_cluster = h->first_cluster;
  h->file_size = entry->file_size;
  h->offset = 0;
  h->is_dir = (entry->attr & ATTR_DIRECTORY) != 0;
  h->dir_entry = entry;

  return h;
}

static int fat32_read(void *file, char *buf, size_t size) {
  Fat32FileHandle *h = (Fat32FileHandle *)file;
  if (!h || h->is_dir)
    return -1;

  if (h->offset >= h->file_size)
    return 0;

  size_t remaining = h->file_size - h->offset;
  size_t to_read = size < remaining ? size : remaining;
  size_t read_bytes = 0;

  while (read_bytes < to_read) {
    uint32_t cluster_offset = h->offset % h->vol->bytes_per_cluster;
    uint32_t chunk = h->vol->bytes_per_cluster - cluster_offset;
    if (chunk > to_read - read_bytes)
      chunk = to_read - read_bytes;

    uint8_t *cptr = get_cluster_ptr(h->vol, h->curr_cluster);
    if (!cptr)
      break;

    memcpy(buf + read_bytes, cptr + cluster_offset, chunk);

    read_bytes += chunk;
    h->offset += chunk;

    if (h->offset % h->vol->bytes_per_cluster == 0) {
      uint32_t next = read_fat(h->vol, h->curr_cluster);
      if (next >= 0x0FFFFFF8)
        break;
      h->curr_cluster = next;
    }
  }

  return (int)read_bytes;
}

static int fat32_write(void *file, const char *buf, size_t size) {
  Fat32FileHandle *h = (Fat32FileHandle *)file;
  if (!h || h->is_dir)
    return -1;

  size_t written_bytes = 0;

  if (h->first_cluster == 0) {
    uint32_t new_c = alloc_cluster(h->vol);
    if (!new_c)
      return -1;
    h->first_cluster = new_c;
    h->curr_cluster = new_c;
    if (h->dir_entry) {
      h->dir_entry->first_cluster_low = new_c & 0xFFFF;
      h->dir_entry->first_cluster_high = (new_c >> 16) & 0xFFFF;
    }
  }

  while (written_bytes < size) {
    uint32_t cluster_offset = h->offset % h->vol->bytes_per_cluster;
    uint32_t chunk = h->vol->bytes_per_cluster - cluster_offset;
    if (chunk > size - written_bytes)
      chunk = size - written_bytes;

    uint8_t *cptr = get_cluster_ptr(h->vol, h->curr_cluster);
    if (!cptr)
      break;

    memcpy(cptr + cluster_offset, buf + written_bytes, chunk);

    written_bytes += chunk;
    h->offset += chunk;

    if (h->offset > h->file_size) {
      h->file_size = h->offset;
      if (h->dir_entry) {
        h->dir_entry->file_size = h->file_size;
      }
    }

    if (h->offset % h->vol->bytes_per_cluster == 0 && written_bytes < size) {
      uint32_t next = read_fat(h->vol, h->curr_cluster);
      if (next >= 0x0FFFFFF8) {
        uint32_t new_c = alloc_cluster(h->vol);
        if (!new_c)
          break;
        write_fat(h->vol, h->curr_cluster, new_c);
        next = new_c;
      }
      h->curr_cluster = next;
    }
  }

  return (int)written_bytes;
}

static void fat32_close(void *file) {
  if (file)
    heap::kfree(file);
}

static bool fat32_init(FileSystem *fs) {
  (void)fs;
  return true;
}

FileSystem filesystem = {.name = "fat32",
                         .init = fat32_init,
                         .open = fat32_open,
                         .read = fat32_read,
                         .write = fat32_write,
                         .close = fat32_close,
                         .list = nullptr,
                         .ioctl = nullptr};

bool mount_ramdisk(void *addr, size_t size) {
  if (!addr || size < 512)
    return false;

  Fat32Volume *vol = (Fat32Volume *)heap::kmalloc(sizeof(Fat32Volume));
  if (!vol)
    return false;

  memset(vol, 0, sizeof(Fat32Volume));
  vol->disk_image = (uint8_t *)addr;
  vol->disk_size = size;

  memcpy(&vol->bpb, addr, sizeof(BPB32));

  if (vol->bpb.bytes_per_sector == 0)
    vol->bpb.bytes_per_sector = 512;

  vol->bytes_per_cluster = vol->bpb.bytes_per_sector * vol->bpb.sectors_per_cluster;
  vol->fat_start_sector = vol->bpb.reserved_sector_count;
  vol->data_start_sector =
      vol->bpb.reserved_sector_count + (vol->bpb.num_fats * vol->bpb.fat_size_32);
  vol->root_cluster = vol->bpb.root_cluster;

  main_vol = vol;

  drivers::serial::writef(
      "FAT32: Mounted volume size=%zu bytes_per_sec=%u cluster_sec=%u root_cluster=%u\n", size,
      vol->bpb.bytes_per_sector, vol->bpb.sectors_per_cluster, vol->root_cluster);

  return true;
}

} // namespace fs::fat32
