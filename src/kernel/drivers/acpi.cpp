#include "LTOS/drivers/acpi.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include <string.h>

namespace drivers::acpi {

static uint32_t pm1a_cnt_port = 0x604;
static bool acpi_initialized = false;

static void outw(uint16_t port, uint16_t val) {
  asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static bool check_rsdp(const char *ptr) {
  if (memcmp(ptr, "RSD PTR ", 8) != 0)
    return false;

  uint8_t sum = 0;
  for (int i = 0; i < 20; i++)
    sum += ptr[i];

  return sum == 0;
}

void init() {
  if (acpi_initialized)
    return;

  acpi_initialized = true;

  // Search BIOS ROM area 0xE0000 - 0xFFFFF for RSDP
  const char *rsdp_search = (const char *)0xE0000;
  const RSDPDescriptor *rsdp = nullptr;

  for (size_t i = 0; i < 0x20000; i += 16) {
    if (check_rsdp(rsdp_search + i)) {
      rsdp = (const RSDPDescriptor *)(rsdp_search + i);
      break;
    }
  }

  if (!rsdp) {
    drivers::serial::write("ACPI: RSDP not found, using QEMU/Bochs defaults\n");
    return;
  }

  drivers::serial::writef("ACPI: RSDP found at %lx, RSDT at %x\n", (uint64_t)rsdp,
                          rsdp->rsdt_address);

  const ACPISDTHeader *rsdt = (const ACPISDTHeader *)(uintptr_t)rsdp->rsdt_address;
  if (!rsdt || memcmp(rsdt->signature, "RSDT", 4) != 0) {
    drivers::serial::write("ACPI: Invalid RSDT\n");
    return;
  }

  size_t entries = (rsdt->length - sizeof(ACPISDTHeader)) / 4;
  const uint32_t *table_ptrs = (const uint32_t *)((const char *)rsdt + sizeof(ACPISDTHeader));

  for (size_t i = 0; i < entries; i++) {
    const ACPISDTHeader *header = (const ACPISDTHeader *)(uintptr_t)table_ptrs[i];
    if (header && memcmp(header->signature, "FACP", 4) == 0) {
      const FADT *fadt = (const FADT *)header;
      if (fadt->pm1a_cnt_blk != 0) {
        pm1a_cnt_port = fadt->pm1a_cnt_blk;
        drivers::serial::writef("ACPI: Found FADT, PM1a_CNT_BLK = %x\n", pm1a_cnt_port);
      }
      break;
    }
  }
}

void poweroff() {
  init();

  drivers::serial::write("ACPI: Powering off system...\n");

  // QEMU / Bochs ACPI poweroff
  outw((uint16_t)pm1a_cnt_port, 0x2000);
  outw(0x604, 0x2000);
  outw(0xB004, 0x2000);
  outw(0x600, 0x3400);

  // VirtualBox / QEMU fallback
  outw(0x4004, 0x3400);

  while (true) {
    asm volatile("cli; hlt");
  }
}

void reboot() {
  drivers::serial::write("ACPI: Rebooting system...\n");

  // 8042 keyboard controller CPU reset
  uint8_t good = 0x02;
  while (good & 0x02)
    good = inb(0x64);
  outb(0x64, 0xFE);

  // Fallback: Triple Fault
  struct IDTR {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed)) null_idtr = {0, 0};

  asm volatile("lidt %0; int3" : : "m"(null_idtr));

  while (true) {
    asm volatile("cli; hlt");
  }
}

} // namespace drivers::acpi
