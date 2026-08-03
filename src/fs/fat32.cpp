#include "LTOS/fs/fat32.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include <string.h>

extern "C" char *strchr(const char *s, int c);

namespace fs::fat32 {

struct Fat32Volume {
  Backend backend;

  // RAM-backend convenience (kept so the original in-memory mount path
  // stays a straight memcpy and avoids going through the cluster cache
  // for trivial whole-image access).
  uint8_t *disk_image;
  size_t disk_size;

  BPB32 bpb;
  uint32_t bytes_per_cluster;
  uint32_t fat_start_sector;
  uint32_t data_start_sector;
  uint32_t root_cluster;

  // Cluster cache for non-RAM backends. Holds `bytes_per_cluster` bytes
  // of the most recently touched data-region cluster. Anything outside
  // the data region (BPB, FAT, root directory on FAT12/16) is fetched
  // one sector at a time into `sector_buf`. `cluster_dirty` /
  // `sector_dirty` track whether the cached contents have been mutated
  // in memory and need flushing before being evicted -- without this
  // flag, writes via fat32_write would be silently lost the moment any
  // other cluster is touched.
  uint32_t cached_cluster;
  bool cluster_dirty;
  uint8_t *cluster_buf;
  uint32_t cached_sector;
  bool sector_dirty;
  uint8_t *sector_buf;
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

// Fast path: when the volume is fully memory-resident, point straight
// into the image. Skips the cache indirection entirely -- important for
// the boot-time rootfs-tar case where the same image is read millions of
// times.
static uint8_t *ram_sector_ptr(Fat32Volume *vol, uint32_t sector) {
  uint64_t offset = (uint64_t)sector * vol->bpb.bytes_per_sector;
  if (offset >= vol->disk_size)
    return nullptr;
  return vol->disk_image + offset;
}

static bool read_sectors(Fat32Volume *vol, uint32_t sector, uint32_t count, void *buf) {
  if (vol->backend.read_sectors)
    return vol->backend.read_sectors(vol->backend.ctx, sector, count, buf);
  return false;
}

static bool write_sectors(Fat32Volume *vol, uint32_t sector, uint32_t count, const void *buf) {
  if (vol->backend.write_sectors)
    return vol->backend.write_sectors(vol->backend.ctx, sector, count, buf);
  return false;
}

// Pull a single sector into `vol->sector_buf` (512 bytes, lazily alloc'd).
// Used for FAT table reads, the BPB, and any other non-cluster sector
// access where the cluster cache does not apply.
static uint8_t *cached_sector_ptr(Fat32Volume *vol, uint32_t sector) {
  if (sector == vol->cached_sector && vol->sector_buf) {
    // Latched back as clean after a successful read -- subsequent
    // modifications through the returned pointer re-dirty it.
    vol->sector_dirty = false;
    return vol->sector_buf;
  }

  if (vol->sector_buf && vol->sector_dirty && vol->cached_sector != (uint32_t)-1) {
    // About to overwrite the buffer with a different sector -- flush
    // first or the dirty contents vanish silently.
    write_sectors(vol, vol->cached_sector, 1, vol->sector_buf);
    vol->sector_dirty = false;
  }

  if (!vol->sector_buf) {
    vol->sector_buf = (uint8_t *)heap::kmalloc(vol->bpb.bytes_per_sector);
    if (!vol->sector_buf)
      return nullptr;
  }

  if (!read_sectors(vol, sector, 1, vol->sector_buf))
    return nullptr;

  vol->cached_sector = sector;
  vol->sector_dirty = false;
  return vol->sector_buf;
}

// Cluster cache: every data-region cluster access goes through here.
// One cluster at a time is sufficient -- directory walks and file reads
// both touch sectors within a single cluster for long stretches, so the
// hit rate stays near 100% and a single AHCI multi-sector read per
// cluster is the only I/O we actually issue.
static uint8_t *cached_cluster_ptr(Fat32Volume *vol, uint32_t cluster) {
  if (cluster < 2)
    return nullptr;

  if (cluster == vol->cached_cluster && vol->cluster_buf) {
    vol->cluster_dirty = false;
    return vol->cluster_buf;
  }

  if (vol->cluster_buf && vol->cluster_dirty && vol->cached_cluster >= 2) {
    // Flush pending writes for the previously cached cluster before we
    // reuse the buffer for another. Without this, fat32_write's cluster
    // updates vanish the moment any other cluster is read in.
    uint32_t prev_sector =
        vol->data_start_sector + (vol->cached_cluster - 2) * vol->bpb.sectors_per_cluster;
    write_sectors(vol, prev_sector, vol->bpb.sectors_per_cluster, vol->cluster_buf);
    vol->cluster_dirty = false;
  }

  if (!vol->cluster_buf) {
    vol->cluster_buf = (uint8_t *)heap::kmalloc(vol->bytes_per_cluster);
    if (!vol->cluster_buf)
      return nullptr;
  }

  uint32_t sector = vol->data_start_sector + (cluster - 2) * vol->bpb.sectors_per_cluster;
  if (!read_sectors(vol, sector, vol->bpb.sectors_per_cluster, vol->cluster_buf))
    return nullptr;

  vol->cached_cluster = cluster;
  vol->cluster_dirty = false;
  return vol->cluster_buf;
}

static uint8_t *get_sector_ptr(Fat32Volume *vol, uint32_t sector) {
  if (vol->disk_image)
    return ram_sector_ptr(vol, sector);

  // Figure out which region this sector lives in. Reserved/FAT regions
  // are accessed sparsely (BPB once, FAT entries scattered) so a
  // single-sector cache is the right shape. Data-region sectors come in
  // clusters, which is what the cluster cache is sized for.
  if (sector < vol->data_start_sector)
    return cached_sector_ptr(vol, sector);

  uint32_t rel = sector - vol->data_start_sector;
  uint32_t cluster = (rel / vol->bpb.sectors_per_cluster) + 2;
  uint32_t offset_in_cluster = (rel % vol->bpb.sectors_per_cluster) * vol->bpb.bytes_per_sector;

  uint8_t *cluster_data = cached_cluster_ptr(vol, cluster);
  if (!cluster_data)
    return nullptr;

  return cluster_data + offset_in_cluster;
}

static uint8_t *get_cluster_ptr(Fat32Volume *vol, uint32_t cluster) {
  if (cluster < 2)
    return nullptr;

  if (vol->disk_image) {
    uint32_t sector = vol->data_start_sector + (cluster - 2) * vol->bpb.sectors_per_cluster;
    return ram_sector_ptr(vol, sector);
  }

  return cached_cluster_ptr(vol, cluster);
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
  for (uint32_t f = 0; f < vol->bpb.num_fats; f++) {
    uint32_t fat_sector = vol->fat_start_sector + (f * vol->bpb.fat_size_32) +
                          (fat_offset / vol->bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % vol->bpb.bytes_per_sector;

    uint8_t *sec = get_sector_ptr(vol, fat_sector);
    if (sec) {
      uint32_t existing = *(uint32_t *)(sec + entry_offset);
      *(uint32_t *)(sec + entry_offset) = (existing & 0xF0000000) | (value & 0x0FFFFFFF);

      // RAM backend has the FATs inline with the image; block-dev
      // backends live behind a 1-sector cache, so we have to push the
      // updated FAT sector back to disk or the change is lost the next
      // time the cache is evicted. We mark the sector dirty first so
      // that any later cache eviction also writes it back (covers the
      // case where cached_sector_ptr() flushes a stale buffer).
      if (!vol->disk_image) {
        if (vol->cached_sector == fat_sector)
          vol->sector_dirty = true;
        if (f == 0)
          write_sectors(vol, fat_sector, 1, sec);
      }
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

    // get_cluster_ptr() resets dirty=false on a hit (it's the cache-load
    // path), so re-mark after every write that actually mutates the
    // buffer. Otherwise the next read on a different cluster would
    // evict this one without flushing.
    h->vol->cluster_dirty = true;

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

// Flat directory listing used by the kernel `ls` command. Returns the
// short (8.3) names of every non-long-name, non-deleted entry in the
// root directory. Caps at 256 entries / 1KB so a stray corrupted FAT
// can't blow up the kernel heap.
static void fat32_list(void *vol_ptr) {
  Fat32Volume *vol = (Fat32Volume *)vol_ptr;
  if (!vol)
    vol = main_vol;
  if (!vol)
    return;

  uint32_t curr = vol->root_cluster;
  char line[64];
  size_t count = 0;

  while (curr >= 2 && curr < 0x0FFFFFF8 && count < 256) {
    uint8_t *cluster_data = get_cluster_ptr(vol, curr);
    if (!cluster_data)
      break;

    DirectoryEntry *entries = (DirectoryEntry *)cluster_data;
    size_t num_entries = vol->bytes_per_cluster / sizeof(DirectoryEntry);

    for (size_t i = 0; i < num_entries; i++) {
      if ((uint8_t)entries[i].name[0] == 0x00)
        return;
      if ((uint8_t)entries[i].name[0] == 0xE5)
        continue;
      if (entries[i].attr == ATTR_LONG_NAME)
        continue;
      if (entries[i].attr & ATTR_VOLUME_ID)
        continue;

      size_t name_len = 0;
      while (name_len < 8 && entries[i].name[name_len] != ' ')
        line[name_len] = entries[i].name[name_len], name_len++;

      size_t ext_len = 0;
      while (ext_len < 3 && entries[i].name[8 + ext_len] != ' ')
        line[name_len + 1 + ext_len] = entries[i].name[8 + ext_len], ext_len++;

      if (ext_len) {
        line[name_len] = '.';
        line[name_len + 1 + ext_len] = 0;
      } else {
        line[name_len] = 0;
      }

      kprintf("%s%s\n", line, (entries[i].attr & ATTR_DIRECTORY) ? "/" : "");
      count++;
    }

    curr = read_fat(vol, curr);
  }
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
                         .list = fat32_list,
                         .ioctl = nullptr};

static bool init_volume_from_image(Fat32Volume *vol) {
  if (vol->bpb.bytes_per_sector == 0)
    vol->bpb.bytes_per_sector = 512;

  vol->bytes_per_cluster = vol->bpb.bytes_per_sector * vol->bpb.sectors_per_cluster;
  vol->fat_start_sector = vol->bpb.reserved_sector_count;
  vol->data_start_sector =
      vol->bpb.reserved_sector_count + (vol->bpb.num_fats * vol->bpb.fat_size_32);
  vol->root_cluster = vol->bpb.root_cluster;

  return vol->bytes_per_cluster >= 512 && vol->data_start_sector > 0;
}

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

  if (!init_volume_from_image(vol)) {
    heap::kfree(vol);
    return false;
  }

  main_vol = vol;

  drivers::serial::writef(
      "FAT32: Mounted ramdisk size=%zu bytes_per_sec=%u cluster_sec=%u root_cluster=%u\n",
      size, vol->bpb.bytes_per_sector, vol->bpb.sectors_per_cluster, vol->root_cluster);

  return true;
}

void *mount_backend(const Backend &backend) {
  drivers::serial::writef("FAT32: mount_backend called\n");
  if (!backend.read_sectors)
    return nullptr;

  Fat32Volume *vol = (Fat32Volume *)heap::kmalloc(sizeof(Fat32Volume));
  if (!vol)
    return nullptr;

  memset(vol, 0, sizeof(Fat32Volume));
  vol->backend = backend;
  vol->disk_image = nullptr;

  uint8_t boot_sector[512];
  drivers::serial::writef("FAT32: reading boot sector\n");
  if (!vol->backend.read_sectors(vol->backend.ctx, 0, 1, boot_sector)) {
    drivers::serial::writef("FAT32: boot sector read failed\n");
    heap::kfree(vol);
    return nullptr;
  }
  drivers::serial::writef("FAT32: boot sector read OK\n");

  memcpy(&vol->bpb, boot_sector, sizeof(BPB32));

  if (!init_volume_from_image(vol)) {
    drivers::serial::writef("FAT32: init_volume_from_image failed\n");
    heap::kfree(vol);
    return nullptr;
  }
  drivers::serial::writef("FAT32: init OK\n");

  // Keep main_vol as a fallback for code paths (legacy callers, etc.)
  // that open without supplying a volume handle. Newly-mounted volumes
  // take precedence in the handle returned here.
  main_vol = vol;

  drivers::serial::writef(
      "FAT32: Mounted block-device volume bytes_per_sec=%u cluster_sec=%u root_cluster=%u\n",
      vol->bpb.bytes_per_sector, vol->bpb.sectors_per_cluster, vol->root_cluster);

  return vol;
}

} // namespace fs::fat32
