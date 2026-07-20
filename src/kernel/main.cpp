#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/arch/x86_64/paging.hpp"

#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/psf.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty.hpp"

#include "LTOS/fs/tarfs.hpp"
#include "LTOS/fs/vfs.hpp"

#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"

#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/mm/vmm.hpp"

#include "LTOS/sched/scheduler.hpp"
#include "LTOS/state.hpp"

#include "LTOS/panic.hpp"

#include "LTOS_gen/version.h"
#include "multiboot.h"

#include <cstdint>

bool state::vfs_initialized = false;

extern "C" void kernel_main(uint64_t magic, uint64_t mbi_addr) {

  asm volatile("cli");

  //
  // Serial
  //
  drivers::serial::init();
  drivers::serial::write("Reached kernel_main()!\n");

  //
  // Multiboot modules
  //
  if (magic == 0x36d76289) {

    uint32_t mbi_total_size = *(uint32_t *)mbi_addr;

    paging::reserve_below(mbi_addr + mbi_total_size);

    multiboot2::parse_info(mbi_addr);

    psf::find_font(mbi_addr);

  } else {
    kprintf("Invalid multiboot2 magic!\n");
  }

  //
  // CPU setup
  //
  drivers::pic::init();
  timer::init(250);

  logger::info("PIC, PIT Initialized");

  gdt::init();
  logger::info("GDT Initialized");

  idt::init();
  logger::info("IDT Initialized");

  //
  // Memory
  //
  pmm::init(mbi_addr);
  logger::info("PMM Initialized");

  vmm::init(paging::kernel_pml4);
  logger::info("VMM Initialized");

  paging::init();
  logger::info("Paging Initialized");

  paging::setup_kernel_identity();
  paging::enable_paging();

  logger::info("Paging Enabled");

  //
  // Framebuffer mapping
  //
  {
    uint64_t fb_base = (uint64_t)framebuffer::info.address;
    uint64_t fb_size = (uint64_t)framebuffer::info.pitch * framebuffer::info.height;

    uint64_t fb_start = fb_base & ~0xFFFULL;
    uint64_t fb_end = (fb_base + fb_size + 0xFFF) & ~0xFFFULL;

    for (uint64_t addr = fb_start; addr < fb_end; addr += 0x1000) {

      paging::map_page(paging::kernel_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    logger::info("Framebuffer mapped");
  }

  //
  // Heap
  //
  heap::init();

  logger::info("Heap Initialized, size: %d KiB", heap::HEAP_SIZE / 1024);

  //
  // VFS
  //
  fs::vfs::init();

  logger::info("VFS Initialized");

  //
  // TARFS
  //
  fs::tarfs::mount_vfs();

  //
  // Drivers
  //
  framebuffer::init_backbuffer();

  logger::info("Framebuffer Initialized");

  tty::init();

  logger::info("TTY Initialized");

  console::init();

  logger::info("Console Initialized");

  //
  // Scheduler
  //
  sched::init();

  logger::info("Scheduler initialized");

  logger::info("LosTacOS v%s booted", LTOS_VERSION);
  //
  // Tests
  //
  logger::info("Starting tests..");

  int *test1 = new int;

  *test1 = 2137;

  logger::test("Heap: test1 = %d", *test1);

  auto hello = fs::vfs::find("/bin/init");

  if (!hello) {
    logger::error("ELF: /bin/init not found");
  } else {
    sched::exec("/bin/init");
  }

  asm volatile("sti");

  while (true) {
    asm volatile("hlt");
  }

  panic::halt("kernel_main returned");
}
