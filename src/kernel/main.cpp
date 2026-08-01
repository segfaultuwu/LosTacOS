#include "LTOS/arch/x86_64/cpu.hpp"
#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/mm/paging.hpp"

#include "LTOS/drivers/ahci.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/pci.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/psf.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty.hpp"

#include "LTOS/fs/devfs.hpp"
#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/procfs.hpp"
#include "LTOS/fs/sysfs.hpp"
#include "LTOS/fs/tarfs.hpp"
#include "LTOS/fs/vfs.hpp"

#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/mm/vmm.hpp"

#include "LTOS/sched/scheduler.hpp"
#include "LTOS/state.hpp"

#include "multiboot.h"

#include "LTOS/arch/x86_64/sse.hpp"

#include <cstddef>
#include <stdint.h>

namespace multiboot2 {
void mount_rootfs();
}

bool state::vfs_initialized = false;

extern "C" void kernel_main(uint64_t magic, uint64_t mbi_addr) {
  asm volatile("cli");

  //
  // Early debug
  //

  drivers::serial::init();

  drivers::serial::writef("kernel_main");

  //
  // Boot info
  //

  multiboot2::parse_info(mbi_addr);

  //
  // CPU
  //

  drivers::pic::init();
  timer::init(250);

  gdt::init();
  idt::init();

  sse_init();

  //
  // Memory
  //

  pmm::init(mbi_addr);
  vmm::init(paging::kernel_pml4);
  paging::init();
  paging::setup_kernel_identity();
  paging::enable_paging();

  //
  // Heap
  //

  heap::init();

  //
  // Load GZIP compressed font & rootfs after Heap is ready
  //
  psf::find_font(mbi_addr);
  multiboot2::mount_rootfs();

  arch::cpu::init(mbi_addr);

  //
  // Graphics
  //

  framebuffer::init_backbuffer();
  console::init();

  //
  // Filesystem
  //

  fs::vfs::init();

  fs::tarfs::mount_vfs();

  fs::vfs::create_dir_path("/proc");
  fs::vfs::create_dir_path("/dev");

  fs::mount("/proc", &fs::procfs::filesystem, nullptr);
  fs::mount("/dev", &fs::devfs::filesystem, nullptr);
  fs::mount("/sys", &fs::sysfs::filesystem, nullptr);

  fs::devfs::init_null();
  fs::devfs::init_fb();

  //
  // Devices
  //

  tty::init();

  drivers::pci::init();
  drivers::ahci::init();

  //
  // Scheduler
  //

  sched::init();

  //
  // Userland
  //

  // logger::info("LosTacOS %s", LTOS_VERSION);
  sched::spawn("/bin/init", 0);

  asm volatile("sti");

  while (true)
    asm volatile("hlt");
}
