#include "LTOS/exec/elf.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"

#include <string.h>

namespace elf {

uint64_t load(const char *path) {

  auto node = fs::vfs::find(path);

  if (!node) {
    logger::error("ELF: %s not found", path);
    return 0;
  }

  char *data = fs::vfs::get_content(node);

  if (!data) {
    logger::error("ELF: %s has no content", path);
    return 0;
  }

  Elf64_Ehdr *hdr = (Elf64_Ehdr *)data;

  if (hdr->e_ident[0] != 0x7f || hdr->e_ident[1] != 'E' || hdr->e_ident[2] != 'L' ||
      hdr->e_ident[3] != 'F') {
    logger::error("Not ELF");
    return 0;
  }

  logger::info("ELF entry: %x", hdr->e_entry);

  Elf64_Phdr *ph = (Elf64_Phdr *)(data + hdr->e_phoff);

  for (int i = 0; i < hdr->e_phnum; i++) {

    if (ph[i].p_type != PT_LOAD)
      continue;

    logger::info("LOAD %x size %x", ph[i].p_vaddr, ph[i].p_memsz);

    uint64_t vaddr = ph[i].p_vaddr;
    uint64_t memsz = ph[i].p_memsz;
    uint64_t filesz = ph[i].p_filesz;

    // Tasks currently all run in ring 0 sharing the kernel's own address
    // space (paging::kernel_pml4) -- there's no per-task CR3 switch in the
    // scheduler yet -- and setup_kernel_identity() only identity-maps the
    // low 256 MiB up front. Make sure every page this segment touches is
    // actually present (identity: va == pa) before copying into it, rather
    // than assuming it's already mapped.
    uint64_t page = vaddr & ~0xFFFULL;
    uint64_t end = (vaddr + memsz + 0xFFF) & ~0xFFFULL;

    for (; page < end; page += 0x1000) {
      paging::map_page(paging::kernel_pml4, page, page, PAGE_PRESENT | PAGE_WRITABLE);
    }

    memcpy((void *)vaddr, data + ph[i].p_offset, filesz);

    logger::info("AFTER COPY %x %x %x %x %x", ((uint8_t *)vaddr)[0], ((uint8_t *)vaddr)[1],
                 ((uint8_t *)vaddr)[2], ((uint8_t *)vaddr)[3], ((uint8_t *)vaddr)[4]);

    // Zero-fill .bss (the part of the segment beyond the file's content).
    if (memsz > filesz)
      memset((void *)(vaddr + filesz), 0, memsz - filesz);
  }

  return hdr->e_entry;
}

} // namespace elf
