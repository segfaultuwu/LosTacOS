#include "LTOS/exec/elf.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/paging.hpp"

#include <stdint.h>
#include <string.h>

namespace elf {

uint64_t load(const char *path, mm::AddressSpace *space) {

  auto node = fs::vfs::find(path);

  if (!node) {
    return 0;
  }

  if (!space || !space->table) {
    logger::error("ELF: %s has no address space", path);
    return 0;
  }

  uint64_t *pml4 = space->table->pml4;

  char *data = fs::vfs::get_content(node);

  if (!data) {
    logger::error("ELF: %s has no content", path);
    return 0;
  }

  Elf64_Ehdr *hdr = (Elf64_Ehdr *)data;

  if (memcmp(hdr->e_ident,
             "\x7F"
             "ELF",
             4) != 0) {
    logger::error("Not ELF");
    return 0;
  }

  Elf64_Phdr *ph = (Elf64_Phdr *)(data + hdr->e_phoff);

  uint64_t min_vaddr = UINT64_MAX;
  uint64_t max_vaddr = 0;

  for (int i = 0; i < hdr->e_phnum; i++) {
    if (ph[i].p_type != PT_LOAD)
      continue;

    if (ph[i].p_vaddr < min_vaddr)
      min_vaddr = ph[i].p_vaddr;

    if (ph[i].p_vaddr + ph[i].p_memsz > max_vaddr)
      max_vaddr = ph[i].p_vaddr + ph[i].p_memsz;
  }

  min_vaddr &= ~0xFFFULL;
  max_vaddr = (max_vaddr + 0xFFF) & ~0xFFFULL;

  uint64_t size = max_vaddr - min_vaddr;

  uint64_t base = 0;

  for (int i = 0; i < hdr->e_phnum; i++) {

    if (ph[i].p_type != PT_LOAD)
      continue;

    uint64_t seg_vaddr = ph[i].p_vaddr;
    uint64_t seg_filesz = ph[i].p_filesz;
    uint64_t seg_offset = ph[i].p_offset;

    uint64_t seg_start = seg_vaddr & ~0xFFFULL;
    uint64_t seg_end = (seg_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFFULL;

    for (uint64_t addr = seg_start; addr < seg_end; addr += 0x1000) {

      void *page = paging::alloc_page(); // zeroed already -- covers .bss

      if (!page) {
        logger::error("OOM");
        return 0;
      }

      uint64_t phys = (uint64_t)page;

      paging::map_page(space->table->pml4, addr, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
      void *dst = page;

      // Copy whatever slice of the file's content overlaps this page.
      // The file-backed range within this segment is
      // [seg_vaddr, seg_vaddr + seg_filesz); write through the
      // physical page directly since `pml4` isn't active in CR3 yet.
      uint64_t page_start = addr;
      uint64_t page_end = addr + 0x1000;

      uint64_t copy_start = seg_vaddr > page_start ? seg_vaddr : page_start;
      uint64_t copy_end = (seg_vaddr + seg_filesz) < page_end ? (seg_vaddr + seg_filesz) : page_end;

      if (copy_end > copy_start) {
        uint64_t file_off = seg_offset + (copy_start - seg_vaddr);
        uint64_t page_off = copy_start - page_start;
        uint64_t len = copy_end - copy_start;

        memcpy((uint8_t *)dst + page_off, data + file_off, len);
      }
    }
  }

  uint64_t entry = hdr->e_entry;

  // logger::info("ELF entry raw: %x", hdr->e_entry);

  // logger::info("ELF loaded at %x entry %x", base, entry);

  return entry;
}

} // namespace elf
