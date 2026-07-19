#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/boot.hpp"
#include "LTOS/drivers/psf.hpp"
#include "LTOS/fs/tarfs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/panic.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS_gen/version.h"
#include "multiboot.h"
#include <cstdint>

// void test_task1() {
//   while (true) {
//     logger::info("1");
//     sched::yield();
//   }
// }

// void test_task2() {
//   while (true) {
//     logger::info("2");
//     sched::yield();
//   }
// }

extern "C" void kernel_main(uint64_t magic, uint64_t mbi_addr) {
  fs::vfs::init();
  if (magic == 0x36d76289) {
    uint32_t mbi_total_size = *(uint32_t *)mbi_addr;
    paging::reserve_below(mbi_addr + mbi_total_size);
  }

  struct multiboot_module mods[8];

  if (magic == 0x36d76289) {
    multiboot2::parse_info(mbi_addr);
    psf::find_font(mbi_addr);
  } else {
    kprintf("Invalid multiboot2 magic!\n");
  }

  fs::tarfs::mount_vfs();
  logger::info("VFS Initialized");
  int setup_result = boot::setup(mbi_addr);

  if (setup_result != 0) {
    panic::halt("boot::setup() failed");
  }

  // Multiboot debug shit
  // kprintf("magic=%x mbi=%x\n", magic, mbi_addr);

  // module declarations
  logger::info("LosTacOS v%s booted\n", LTOS_VERSION);
  /* Idt testing shit
   *
   * logger::info("Trying to divide by 0..");
   * volatile int a = 1;
   * volatile int b = 0;
   * volatile int tmp = a / b;
   * kprintf("result: %d", tmp);
   *
   * Timer testing shit
   *
   * while (true) {
   *   if (timer::ticks() % 100 == 0)
   *     logger::info("1 second");
   *
   *   asm volatile("hlt");
   * }

    while (true) {
      logger::info("test");
      timer::sleep(1000);
    }
   */

  logger::info("Starting tests..");

  // Heap test
  int *test1 = new int;
  *test1 = 2137;
  logger::test("Heap: test1 = %d", *test1);

  kprintf("ROOT:\n");
  fs::vfs::list_dir(fs::vfs::root);

  auto bin = fs::vfs::find("/bin");
  auto hello = fs::vfs::find("/bin/hello");

  kprintf("bin=%x\n", (uint64_t)bin);
  kprintf("hello=%x\n", (uint64_t)hello);
  sched::exec("/bin/hello");

  // logger::info("Creating task1");
  // sched::create(test_task1);
  // logger::info("Creating task2");
  // sched::create(test_task2);

  while (true) {
    asm volatile("hlt");
  }

  panic::halt("Should not exit the main loop.");
}
