#pragma once

#include <stdint.h>

enum class Bootloader { Unknown, Grub, Limine };

struct BootInfo {
  Bootloader bootloader;

  uint64_t addr;

  uint64_t multiboot_addr;
  uint64_t limine_addr;
};

extern BootInfo boot_info;
