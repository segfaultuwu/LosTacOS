#include "LTOS/arch/x86_64/cpu.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include <multiboot.h>
#include <string.h>

namespace arch::cpu {

SmpInfo g_smp_info = {};

struct RSDPDescriptor {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
} __attribute__((packed));

struct RSDPDescriptor20 {
  RSDPDescriptor first_part;
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t extended_checksum;
  uint8_t reserved[3];
} __attribute__((packed));

struct ACPISDTHeader {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed));

struct MADTHeader {
  ACPISDTHeader header;
  uint32_t lapic_addr;
  uint32_t flags;
} __attribute__((packed));

struct MADTEntryHeader {
  uint8_t type;
  uint8_t length;
} __attribute__((packed));

struct MADTLapicEntry {
  MADTEntryHeader header;
  uint8_t acpi_processor_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__((packed));

struct MADTIoApicEntry {
  MADTEntryHeader header;
  uint8_t ioapic_id;
  uint8_t reserved;
  uint32_t ioapic_address;
  uint32_t global_system_interrupt_base;
} __attribute__((packed));

static bool validate_checksum(const uint8_t *data, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++)
    sum += data[i];
  return sum == 0;
}

void detect_cpu_info(CpuInfo &cpu, uint32_t id, uint32_t apic_id, uint32_t acpi_id, bool is_bsp) {
  cpu.id = id;
  cpu.apic_id = apic_id;
  cpu.acpi_id = acpi_id;
  cpu.enabled = true;
  cpu.is_bsp = is_bsp;

  // Vendor string
  uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
  cpuid(0, 0, &eax, &ebx, &ecx, &edx);
  uint32_t max_leaf = eax;

  *(uint32_t *)&cpu.vendor[0] = ebx;
  *(uint32_t *)&cpu.vendor[4] = edx;
  *(uint32_t *)&cpu.vendor[8] = ecx;
  cpu.vendor[12] = 0;

  // Family, Model, Stepping
  if (max_leaf >= 1) {
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    cpu.feature_ecx = ecx;
    cpu.feature_edx = edx;

    uint32_t stepping = eax & 0xF;
    uint32_t model = (eax >> 4) & 0xF;
    uint32_t family = (eax >> 8) & 0xF;

    if (family == 6 || family == 15) {
      model |= ((eax >> 16) & 0xF) << 4;
    }
    if (family == 15) {
      family += (eax >> 20) & 0xFF;
    }

    cpu.stepping = stepping;
    cpu.model = model;
    cpu.family = family;
    cpu.logical_cores = (ebx >> 16) & 0xFF;
    if (cpu.logical_cores == 0)
      cpu.logical_cores = 1;
  } else {
    cpu.family = 6;
    cpu.model = 0;
    cpu.stepping = 0;
    cpu.logical_cores = 1;
  }

  // Extended Features (Leaf 7)
  if (max_leaf >= 7) {
    cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    cpu.ext_feature_ebx = ebx;
    cpu.ext_feature_ecx = ecx;
  }

  // Extended Leaves (Leaf 0x80000000)
  cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
  uint32_t max_ext_leaf = eax;

  // Physical core count from 0x80000008
  if (max_ext_leaf >= 0x80000008) {
    cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx);
    cpu.physical_cores = (ecx & 0xFF) + 1;
  } else {
    cpu.physical_cores = cpu.logical_cores;
  }

  // Brand string from 0x80000002, 0x80000003, 0x80000004
  char brand_buf[49] = {};
  if (max_ext_leaf >= 0x80000004) {
    cpuid(0x80000002, 0, (uint32_t *)&brand_buf[0], (uint32_t *)&brand_buf[4],
          (uint32_t *)&brand_buf[8], (uint32_t *)&brand_buf[12]);
    cpuid(0x80000003, 0, (uint32_t *)&brand_buf[16], (uint32_t *)&brand_buf[20],
          (uint32_t *)&brand_buf[24], (uint32_t *)&brand_buf[28]);
    cpuid(0x80000004, 0, (uint32_t *)&brand_buf[32], (uint32_t *)&brand_buf[36],
          (uint32_t *)&brand_buf[40], (uint32_t *)&brand_buf[44]);
    brand_buf[48] = 0;

    // Trim leading spaces
    const char *b = brand_buf;
    while (*b == ' ')
      b++;
    strncpy(cpu.brand, b, sizeof(cpu.brand) - 1);
  }

  if (cpu.brand[0] == 0) {
    ksnprintf(cpu.brand, sizeof(cpu.brand), "%s Processor (Family %u Model %u)",
              cpu.vendor[0] ? cpu.vendor : "x86_64", cpu.family, cpu.model);
  }

  // Build flags string
  size_t offset = 0;
  cpu.flags_str[0] = 0;

  auto add_flag = [&](const char *flag, bool present) {
    if (!present)
      return;
    if (offset > 0 && offset < sizeof(cpu.flags_str) - 1) {
      cpu.flags_str[offset++] = ' ';
    }
    size_t len = strlen(flag);
    if (offset + len < sizeof(cpu.flags_str)) {
      memcpy(cpu.flags_str + offset, flag, len);
      offset += len;
      cpu.flags_str[offset] = 0;
    }
  };

  add_flag("fpu", cpu.feature_edx & (1 << 0));
  add_flag("vme", cpu.feature_edx & (1 << 1));
  add_flag("de", cpu.feature_edx & (1 << 2));
  add_flag("pse", cpu.feature_edx & (1 << 3));
  add_flag("tsc", cpu.feature_edx & (1 << 4));
  add_flag("msr", cpu.feature_edx & (1 << 5));
  add_flag("pae", cpu.feature_edx & (1 << 6));
  add_flag("mce", cpu.feature_edx & (1 << 7));
  add_flag("cx8", cpu.feature_edx & (1 << 8));
  add_flag("apic", cpu.feature_edx & (1 << 9));
  add_flag("sep", cpu.feature_edx & (1 << 11));
  add_flag("mtrr", cpu.feature_edx & (1 << 12));
  add_flag("pge", cpu.feature_edx & (1 << 13));
  add_flag("mca", cpu.feature_edx & (1 << 14));
  add_flag("cmov", cpu.feature_edx & (1 << 15));
  add_flag("pat", cpu.feature_edx & (1 << 16));
  add_flag("pse36", cpu.feature_edx & (1 << 17));
  add_flag("clflush", cpu.feature_edx & (1 << 19));
  add_flag("mmx", cpu.feature_edx & (1 << 23));
  add_flag("fxsr", cpu.feature_edx & (1 << 24));
  add_flag("sse", cpu.feature_edx & (1 << 25));
  add_flag("sse2", cpu.feature_edx & (1 << 26));
  add_flag("ht", cpu.feature_edx & (1 << 28));

  add_flag("pni", cpu.feature_ecx & (1 << 0));
  add_flag("pclmulqdq", cpu.feature_ecx & (1 << 1));
  add_flag("ssse3", cpu.feature_ecx & (1 << 9));
  add_flag("fma", cpu.feature_ecx & (1 << 12));
  add_flag("cx16", cpu.feature_ecx & (1 << 13));
  add_flag("sse4_1", cpu.feature_ecx & (1 << 19));
  add_flag("sse4_2", cpu.feature_ecx & (1 << 20));
  add_flag("x2apic", cpu.feature_ecx & (1 << 21));
  add_flag("movbe", cpu.feature_ecx & (1 << 22));
  add_flag("popcnt", cpu.feature_ecx & (1 << 23));
  add_flag("aes", cpu.feature_ecx & (1 << 25));
  add_flag("xsave", cpu.feature_ecx & (1 << 26));
  add_flag("avx", cpu.feature_ecx & (1 << 28));
  add_flag("f16c", cpu.feature_ecx & (1 << 29));
  add_flag("rdrand", cpu.feature_ecx & (1 << 30));
  add_flag("hypervisor", cpu.feature_ecx & (1 << 31));

  add_flag("bmi1", cpu.ext_feature_ebx & (1 << 3));
  add_flag("avx2", cpu.ext_feature_ebx & (1 << 5));
  add_flag("bmi2", cpu.ext_feature_ebx & (1 << 8));
  add_flag("erms", cpu.ext_feature_ebx & (1 << 9));
  add_flag("avx512f", cpu.ext_feature_ebx & (1 << 16));
}

static MADTHeader *find_madt_from_rsdp(const RSDPDescriptor *rsdp) {
  if (!rsdp)
    return nullptr;

  if (rsdp->revision >= 2) {
    const RSDPDescriptor20 *rsdp20 = (const RSDPDescriptor20 *)rsdp;
    if (rsdp20->xsdt_address != 0) {
      const ACPISDTHeader *xsdt = (const ACPISDTHeader *)rsdp20->xsdt_address;
      if (memcmp(xsdt->signature, "XSDT", 4) == 0 &&
          validate_checksum((const uint8_t *)xsdt, xsdt->length)) {
        size_t entries = (xsdt->length - sizeof(ACPISDTHeader)) / sizeof(uint64_t);
        const uint64_t *pointers =
            (const uint64_t *)((const uint8_t *)xsdt + sizeof(ACPISDTHeader));
        for (size_t i = 0; i < entries; i++) {
          const ACPISDTHeader *header = (const ACPISDTHeader *)pointers[i];
          if (header && memcmp(header->signature, "APIC", 4) == 0 &&
              validate_checksum((const uint8_t *)header, header->length)) {
            return (MADTHeader *)header;
          }
        }
      }
    }
  }

  if (rsdp->rsdt_address != 0) {
    const ACPISDTHeader *rsdt = (const ACPISDTHeader *)(uint64_t)rsdp->rsdt_address;
    if (memcmp(rsdt->signature, "RSDT", 4) == 0 &&
        validate_checksum((const uint8_t *)rsdt, rsdt->length)) {
      size_t entries = (rsdt->length - sizeof(ACPISDTHeader)) / sizeof(uint32_t);
      const uint32_t *pointers = (const uint32_t *)((const uint8_t *)rsdt + sizeof(ACPISDTHeader));
      for (size_t i = 0; i < entries; i++) {
        const ACPISDTHeader *header = (const ACPISDTHeader *)(uint64_t)pointers[i];
        if (header && memcmp(header->signature, "APIC", 4) == 0 &&
            validate_checksum((const uint8_t *)header, header->length)) {
          return (MADTHeader *)header;
        }
      }
    }
  }

  return nullptr;
}

static const RSDPDescriptor *scan_bios_rsdp() {
  for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16) {
    if (memcmp((void *)addr, "RSD PTR ", 8) == 0) {
      if (validate_checksum((const uint8_t *)addr, sizeof(RSDPDescriptor))) {
        return (const RSDPDescriptor *)addr;
      }
    }
  }
  return nullptr;
}

uint32_t lapic_id() {
  if (g_smp_info.lapic_phys_addr == 0)
    return 0;
  volatile uint32_t *lapic_id_reg = (volatile uint32_t *)(g_smp_info.lapic_phys_addr + 0x020);
  return (*lapic_id_reg >> 24) & 0xFF;
}

void lapic_init() {
  if (g_smp_info.lapic_phys_addr == 0)
    g_smp_info.lapic_phys_addr = 0xFEE00000;

  paging::map_page(paging::kernel_pml4, g_smp_info.lapic_phys_addr, g_smp_info.lapic_phys_addr,
                   PAGE_PRESENT | PAGE_WRITABLE);

  volatile uint32_t *svr = (volatile uint32_t *)(g_smp_info.lapic_phys_addr + 0x0F0);
  *svr = 0x1FF;
}

void lapic_eoi() {
  if (g_smp_info.lapic_phys_addr == 0)
    return;
  volatile uint32_t *eoi = (volatile uint32_t *)(g_smp_info.lapic_phys_addr + 0x0B0);
  *eoi = 0;
}

void init(uint64_t mbi_phys_addr) {
  g_smp_info = {};
  g_smp_info.lapic_phys_addr = 0xFEE00000;

  const RSDPDescriptor *rsdp = nullptr;

  multiboot2::for_each_tag(mbi_phys_addr, [&](struct multiboot_tag *tag) {
    if (tag->type == 14 || tag->type == 15) {
      rsdp = (const RSDPDescriptor *)((uint8_t *)tag + 8);
    }
  });

  if (!rsdp) {
    rsdp = scan_bios_rsdp();
  }

  MADTHeader *madt = nullptr;
  if (rsdp) {
    madt = find_madt_from_rsdp(rsdp);
  }

  if (madt) {
    g_smp_info.lapic_phys_addr = madt->lapic_addr;

    uint8_t *ptr = (uint8_t *)madt + sizeof(MADTHeader);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (ptr < end) {
      MADTEntryHeader *entry = (MADTEntryHeader *)ptr;
      if (entry->length == 0)
        break;

      if (entry->type == 0) { // Processor Local APIC
        MADTLapicEntry *lapic_entry = (MADTLapicEntry *)entry;
        if ((lapic_entry->flags & 1) || (lapic_entry->flags & 2)) {
          if (g_smp_info.num_cpus < MAX_CPUS) {
            uint32_t cpu_idx = g_smp_info.num_cpus;
            bool is_bsp = (cpu_idx == 0);
            detect_cpu_info(g_smp_info.cpus[cpu_idx], cpu_idx, lapic_entry->apic_id,
                            lapic_entry->acpi_processor_id, is_bsp);
            g_smp_info.num_cpus++;
          }
        }
      } else if (entry->type == 1) { // IO APIC
        MADTIoApicEntry *ioapic_entry = (MADTIoApicEntry *)entry;
        g_smp_info.ioapic_phys_addr = ioapic_entry->ioapic_address;
      }

      ptr += entry->length;
    }
  }

  if (g_smp_info.num_cpus == 0) {
    detect_cpu_info(g_smp_info.cpus[0], 0, 0, 0, true);
    g_smp_info.num_cpus = 1;
  }

  lapic_init();

  kprintf("CPU: Discovered %u CPU core(s) [LAPIC: 0x%lx | IOAPIC: 0x%lx]\n", g_smp_info.num_cpus,
          g_smp_info.lapic_phys_addr, g_smp_info.ioapic_phys_addr);
  drivers::serial::writef("CPU: Discovered %u CPU core(s) [LAPIC: 0x%lx | IOAPIC: 0x%lx]\n",
                          g_smp_info.num_cpus, g_smp_info.lapic_phys_addr,
                          g_smp_info.ioapic_phys_addr);
  for (uint32_t i = 0; i < g_smp_info.num_cpus; i++) {
    kprintf("  CPU %u: APIC ID %u | %s (%s)\n", i, g_smp_info.cpus[i].apic_id,
            g_smp_info.cpus[i].brand, g_smp_info.cpus[i].vendor);
    drivers::serial::writef("  CPU %u: APIC ID %u | %s (%s)\n", i, g_smp_info.cpus[i].apic_id,
                            g_smp_info.cpus[i].brand, g_smp_info.cpus[i].vendor);
  }
}

} // namespace arch::cpu
