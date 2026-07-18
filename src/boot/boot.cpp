#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/console.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/tty.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/mm/vmm.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/vga.hpp"
#include <cstdint>
namespace boot {

int setup(uint64_t mbi_addr) {
  drivers::serial::init();
  drivers::serial::write("Reached boot::setup()!\n");

  vga::clear();

  logger::info("VGA Initialized");

  drivers::pic::init();
  timer::init(100);
  logger::info("PIC, PIT Initialized");

  gdt::init();
  logger::info("GDT Initialized");
  idt::init();
  logger::info("IDT Initialized");

  pmm::init(mbi_addr);
  logger::info("PMM Initialized");

  vmm::init(paging::kernel_pml4);
  logger::info("VMM Initialized");

  paging::init();
  logger::info("Paging Initialized");
  paging::setup_kernel_identity();
  paging::enable_paging();
  logger::info("Paging Enabled");

  heap::init();
  logger::info("Heap Initialized, size: %d KiB", (heap::HEAP_SIZE / 1024));

  fs::vfs::init();
  logger::info("VFS Initialized");

  tty::init();

  console::init();
  logger::info("Console Initialized");

  asm volatile("sti");
  return 0;
}
} // namespace boot
