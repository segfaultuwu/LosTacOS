#pragma once

#include <stddef.h>
#include <stdint.h>

namespace arch::cpu {

constexpr size_t MAX_CPUS = 256;

struct CpuInfo {
  uint32_t id;             // Index (0, 1, 2, ...)
  uint32_t apic_id;        // Local APIC ID
  uint32_t acpi_id;        // ACPI Processor ID
  bool enabled;            // Is core enabled
  bool is_bsp;             // Is Bootstrap Processor

  char vendor[13];         // e.g. "GenuineIntel", "AuthenticAMD", "TCGTCGTCGTCG"
  char brand[49];          // e.g. "Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz"
  uint32_t family;
  uint32_t model;
  uint32_t stepping;

  uint32_t logical_cores;
  uint32_t physical_cores;

  uint32_t feature_ecx;
  uint32_t feature_edx;
  uint32_t ext_feature_ebx;
  uint32_t ext_feature_ecx;

  char flags_str[512];
};

struct SmpInfo {
  uint64_t lapic_phys_addr;
  uint64_t ioapic_phys_addr;
  uint32_t num_cpus;
  uint32_t bsp_apic_id;
  CpuInfo cpus[MAX_CPUS];
};

extern SmpInfo g_smp_info;

void init(uint64_t mbi_phys_addr);

inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
               : "a"(leaf), "c"(subleaf));
}

void lapic_init();
uint32_t lapic_id();
void lapic_eoi();

void detect_cpu_info(CpuInfo &cpu, uint32_t id, uint32_t apic_id, uint32_t acpi_id, bool is_bsp);

} // namespace arch::cpu
