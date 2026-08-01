#pragma once

#include "LTOS/fs/vfs.hpp"
#include <stdint.h>

namespace fs::fat32 {

struct BPB32 {
  uint8_t jmp[3];
  char oem[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t num_fats;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t fat_size_16;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t total_sectors_32;
  uint32_t fat_size_32;
  uint16_t ext_flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t fs_info;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  uint8_t drive_number;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed));

struct DirectoryEntry {
  char name[11];
  uint8_t attr;
  uint8_t nt_reserved;
  uint8_t creation_time_tenth;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t first_cluster_high;
  uint16_t write_time;
  uint16_t write_date;
  uint16_t first_cluster_low;
  uint32_t file_size;
} __attribute__((packed));

constexpr uint8_t ATTR_READ_ONLY = 0x01;
constexpr uint8_t ATTR_HIDDEN = 0x02;
constexpr uint8_t ATTR_SYSTEM = 0x04;
constexpr uint8_t ATTR_VOLUME_ID = 0x08;
constexpr uint8_t ATTR_DIRECTORY = 0x10;
constexpr uint8_t ATTR_ARCHIVE = 0x20;
constexpr uint8_t ATTR_LONG_NAME = 0x0F;

extern FileSystem filesystem;

bool mount_ramdisk(void *addr, size_t size);

} // namespace fs::fat32
