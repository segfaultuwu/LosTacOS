#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/vga.hpp"
#include <cstdint>
namespace boot {

int setup(uint64_t mbi_addr) {
  drivers::serial::init();
  drivers::serial::write("Reached boot::setup()!\n");

  vga::clear();
  logger::info("Initialized VGA");

  drivers::pic::init();
  timer::init(100);
  logger::info("Initialized PIC, PIT");

  gdt::init();
  logger::info("Initialized GDT");
  idt::init();
  logger::info("Initialized ITD");

  pmm::init(mbi_addr);
  logger::info("Initialized PMM");

  paging::init();
  logger::info("Paging initialized");
  paging::setup_kernel_identity();
  paging::enable_paging();
  logger::info("Paging enabled");

  heap::init();
  logger::info("Heap initialized, size: %d KiB", (heap::HEAP_SIZE / 1024));

  asm volatile("sti");
  return 0;
}
} // namespace boot
