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

// 64-bit TSS descriptor is 16 bytes wide (occupies two normal GDT slots).
struct TSSDescriptor {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t gran;
  uint8_t base_high;
  uint32_t base_upper;
  uint32_t reserved;
} __attribute__((packed));

struct GDTPtr {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

struct TSS {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iopb_offset;
} __attribute__((packed));

struct GDTTable {
  GDTEntry entries[5];
  TSSDescriptor tss_desc;
} __attribute__((packed));

GDTTable gdt_table;
GDTPtr gdtp;

static TSS tss;
alignas(16) static uint8_t kernel_interrupt_stack[32768];

extern "C" void gdt_flush(GDTPtr *);
extern "C" void tss_flush(uint16_t selector);

void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
  gdt_table.entries[i].limit_low = limit & 0xFFFF;
  gdt_table.entries[i].base_low = base & 0xFFFF;
  gdt_table.entries[i].base_mid = (base >> 16) & 0xFF;
  gdt_table.entries[i].access = access;
  gdt_table.entries[i].gran = (limit >> 16) & 0x0F;
  gdt_table.entries[i].gran |= gran & 0xF0;
  gdt_table.entries[i].base_high = (base >> 24) & 0xFF;
}

static void set_tss_entry(uint64_t base, uint32_t limit) {
  TSSDescriptor &d = gdt_table.tss_desc;

  d.limit_low = limit & 0xFFFF;
  d.base_low = base & 0xFFFF;
  d.base_mid = (base >> 16) & 0xFF;
  d.access = 0x89; // present, DPL0, type=9 (64-bit TSS, available)
  d.gran = (limit >> 16) & 0x0F;
  d.base_high = (base >> 24) & 0xFF;
  d.base_upper = (uint32_t)(base >> 32);
  d.reserved = 0;
}

void set_kernel_stack(uint64_t rsp0) {
  tss.rsp0 = rsp0;
}

void init() {
  gdtp.limit = sizeof(gdt_table) - 1;
  gdtp.base = (uint64_t)&gdt_table;

  set_entry(0, 0, 0, 0, 0);

  // kernel
  set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
  set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);

  // user
  set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);
  set_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);

  // TSS
  for (uint32_t i = 0; i < sizeof(TSS); i++)
    ((uint8_t *)&tss)[i] = 0;

  tss.rsp0 = (uint64_t)(kernel_interrupt_stack + sizeof(kernel_interrupt_stack));
  tss.iopb_offset = sizeof(TSS); // no I/O bitmap

  set_tss_entry((uint64_t)&tss, sizeof(TSS) - 1);

  gdt_flush(&gdtp);
  tss_flush(0x28); // selector = index 5 (5*8 = 0x28), the TSS descriptor
}

} // namespace gdt
