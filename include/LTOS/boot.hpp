#pragma once

#include <stdint.h>

enum class Bootloader { Unknown, Grub, Limine };

struct BootInfo {
  Bootloader bootloader;

  uint64_t addr;
  uint64_t multiboot_addr;

  char bootloader_name[128];
};

extern BootInfo boot_info;
