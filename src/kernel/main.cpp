#include "LTOS/arch/x86_64/cpu.hpp"
#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/boot.hpp"
#include "LTOS/fs/fat32.hpp"
#include "LTOS/mm/paging.hpp"

#include "LTOS/drivers/ahci.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/mouse.hpp"
#include "LTOS/drivers/pci.hpp"

#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/psf.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty.hpp"
#include "LTOS/drivers/uhci.hpp"
#include "LTOS/drivers/usb.hpp"

#include "LTOS/fs/devfs.hpp"
#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/isofs.hpp"
#include "LTOS/fs/procfs.hpp"
#include "LTOS/fs/sysfs.hpp"
#include "LTOS/fs/tarfs.hpp"
#include "LTOS/fs/vfs.hpp"

#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/mm/vmm.hpp"

#include "LTOS/logger.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS/state.hpp"

#include "multiboot.h"

#include "LTOS/arch/x86_64/sse.hpp"

#include <stdint.h>
#include <string.h>

namespace multiboot2 {
void mount_rootfs();
}

bool state::vfs_initialized = false;

extern "C" void kernel_main(uint64_t magic, uint64_t info) {
  asm volatile("cli");

  //
  // Early debug
  //

  drivers::serial::init();
  drivers::serial::writef("kernel_main\n");

  //
  // Bootloader shit
  //
  //

  const char *name = multiboot2::get_bootloader_name(info);

  boot_info.multiboot_addr = info;

  if (strstr(name, "Limine")) {
    boot_info.bootloader = Bootloader::Limine;

    drivers::serial::writef("Bootloader: LIMINE (%s)\n", name);
  } else if (strstr(name, "GRUB")) {
    boot_info.bootloader = Bootloader::Grub;

    drivers::serial::writef("Bootloader: GRUB (%s)\n", name);
  } else {
    boot_info.bootloader = Bootloader::Unknown;

    drivers::serial::writef("Bootloader: UNKNOWN (%s)\n", name);
  }

  strncpy(boot_info.bootloader_name, multiboot2::get_bootloader_name(info),
          sizeof(boot_info.bootloader_name));

  //
  // Boot info
  //

  multiboot2::parse_info(boot_info.multiboot_addr);

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

  pmm::init(boot_info.multiboot_addr);
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
  psf::find_font(boot_info.multiboot_addr);
  multiboot2::mount_rootfs();

  arch::cpu::init(boot_info.multiboot_addr);

  //
  // Graphics
  //
  framebuffer::init(boot_info.addr);
  framebuffer::init_backbuffer();
  console::init();

  //
  // Filesystem
  //

  fs::vfs::init();

  fs::tarfs::mount_vfs();

  fs::vfs::create_dir_path("/proc");
  fs::vfs::create_dir_path("/dev");
  fs::vfs::create_dir_path("/sys");

  fs::register_filesystem(&fs::tarfs::filesystem);
  fs::register_filesystem(&fs::procfs::filesystem);
  fs::register_filesystem(&fs::devfs::filesystem);
  fs::register_filesystem(&fs::sysfs::filesystem);
  fs::register_filesystem(&fs::fat32::filesystem);
  fs::register_filesystem(&fs::isofs::filesystem);

  fs::mount("/", &fs::tarfs::filesystem, nullptr);
  fs::mount("/proc", &fs::procfs::filesystem, nullptr);
  fs::mount("/dev", &fs::devfs::filesystem, nullptr);
  fs::mount("/sys", &fs::sysfs::filesystem, nullptr);

  fs::devfs::init_null();
  fs::devfs::init_fb();

  //
  // Devices
  //

  tty::init();
  drivers::mouse::init();
  drivers::mouse::register_dev();

  drivers::pci::init();
  drivers::ahci::init();

  drivers::uhci::init();
  drivers::usb::init();

  for (int port = 0; port < 32; port++) {
    if (drivers::ahci::atapi::is_atapi_device(port)) {
      void *boot_vol = fs::isofs::mount(port);
      if (boot_vol) {
        fs::vfs::create_dir_path("/boot");
        if (fs::mount("/boot", &fs::isofs::filesystem, boot_vol))
          logger::info("Mounted boot ISO at /boot (ATAPI port %d)", port);
        else
          logger::warn("Failed to mount boot ISO at /boot");
      }
      break;
    }
  }

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
