#pragma once

#include "LTOS/fs/vfs.hpp"
#include <stddef.h>
#include <stdint.h>

namespace fs::isofs {

struct IsoDirectoryRecord {
  uint8_t length;
  uint8_t ext_attr_length;
  uint32_t extent_lba_le;
  uint32_t extent_lba_be;
  uint32_t data_length_le;
  uint32_t data_length_be;
  uint8_t date[7];
  uint8_t flags;
  uint8_t file_unit_size;
  uint8_t interleave_gap;
  uint16_t volume_seq_le;
  uint16_t volume_seq_be;
  uint8_t name_len;
  char name[];
} __attribute__((packed));

constexpr uint8_t ISO_FLAG_DIRECTORY = 0x02;

extern FileSystem filesystem;

void *mount(uint8_t atapi_port);

} // namespace fs::isofs
