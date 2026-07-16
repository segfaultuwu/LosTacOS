#pragma once

#include <stdint.h>

#define MULTIBOOT_FLAG_MEM 0x001
#define MULTIBOOT_FLAG_DEVICE 0x002
#define MULTIBOOT_FLAG_CMDLINE 0x004
#define MULTIBOOT_FLAG_MODS 0x008
#define MULTIBOOT_FLAG_AOUT 0x010
#define MULTIBOOT_FLAG_ELF 0x020
#define MULTIBOOT_FLAG_MMAP 0x040
#define MULTIBOOT_FLAG_CONFIG 0x080
#define MULTIBOOT_FLAG_LOADER 0x100
#define MULTIBOOT_FLAG_APM 0x200
#define MULTIBOOT_FLAG_VBE 0x400
#define MULTIBOOT_FLAG_FRAMEBUFFER 0x1000

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

struct multiboot_mmap_entry {
  uint64_t addr;
  uint64_t len;
  uint32_t type;
  uint32_t zero;
};

struct multiboot_tag_mmap {
  uint32_t type;
  uint32_t size;

  uint32_t entry_size;
  uint32_t entry_version;
};

typedef struct multiboot_mmap_entry multiboot_memory_map_t;

typedef struct multiboot_info multiboot_info_t;

struct multiboot_info_header {
  uint32_t total_size;
  uint32_t reserved;
};

struct multiboot_tag {
  uint32_t type;
  uint32_t size;
};

struct multiboot_tag_string {
  uint32_t type;
  uint32_t size;
  char string[0];
};

struct multiboot_tag_module {
  uint32_t type;
  uint32_t size;
  uint32_t mod_start;
  uint32_t mod_end;
  char cmdline[];
};

struct multiboot_module {
  uint32_t start;
  uint32_t end;
  const char *cmdline;
};

#ifdef __cplusplus
namespace multiboot2 {
#endif
void parse_info(uint64_t mbi_phys_addr);

int list_modules(uint64_t mbi_phys_addr, struct multiboot_module *out,
                 int max_count);

#ifdef __cplusplus
}
#endif
