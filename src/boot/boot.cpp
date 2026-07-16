#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/vga.hpp"
namespace boot {
int setup() {
  drivers::serial::init();
  drivers::serial::write("Reached boot::setup()!\n");

  vga::clear();

  drivers::pic::init();
  timer::init(100);

  gdt::init();
  idt::init();

  paging::init();
  paging::setup_kernel_identity();
  paging::enable_paging();

  asm volatile("sti");
  return 0;
}
} // namespace boot
