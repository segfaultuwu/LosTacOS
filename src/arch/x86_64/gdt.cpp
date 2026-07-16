#include "LTOS/arch/x86_64/gdt.hpp"

namespace gdt {

struct GDTEntry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t gran;
  uint8_t base_high;
} __attribute__((packed));

struct GDTPtr {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

GDTEntry gdt[5];
GDTPtr gdtp;

extern "C" void gdt_flush(GDTPtr *);

void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access,
               uint8_t gran) {
  gdt[i].limit_low = limit & 0xFFFF;
  gdt[i].base_low = base & 0xFFFF;
  gdt[i].base_mid = (base >> 16) & 0xFF;
  gdt[i].access = access;
  gdt[i].gran = (limit >> 16) & 0x0F;
  gdt[i].gran |= gran & 0xF0;
  gdt[i].base_high = (base >> 24) & 0xFF;
}

void init() {
  gdtp.limit = sizeof(gdt) - 1;
  gdtp.base = (uint64_t)&gdt;

  set_entry(0, 0, 0, 0, 0);

  // kernel
  set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
  set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);

  // user
  set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);
  set_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);

  gdt_flush(&gdtp);
}

} // namespace gdt
