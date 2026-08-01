#pragma once

#include <stdint.h>

namespace drivers::acpi {

struct RSDPDescriptor {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
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

struct FADT {
  ACPISDTHeader header;
  uint32_t firmware_ctrl;
  uint32_t dsdt;
  uint8_t reserved;
  uint8_t preferred_pm_profile;
  uint16_t sci_interrupt;
  uint32_t smi_command_port;
  uint8_t acpi_enable;
  uint8_t acpi_disable;
  uint8_t s4bios_req;
  uint8_t pstate_control;
  uint32_t pm1a_event_blk;
  uint32_t pm1b_event_blk;
  uint32_t pm1a_cnt_blk;
  uint32_t pm1b_cnt_blk;
  uint32_t pm2_cnt_blk;
  uint32_t pm_timer_blk;
  uint32_t gpe0_blk;
  uint32_t gpe1_blk;
  uint8_t pm1_event_len;
  uint8_t pm1_cnt_len;
  uint8_t pm2_cnt_len;
  uint8_t pm_timer_len;
  uint8_t gpe0_len;
  uint8_t gpe1_len;
  uint8_t gpe1_base;
  uint8_t cstate_control;
  uint16_t worst_c2_latency;
  uint16_t worst_c3_latency;
  uint16_t flush_size;
  uint16_t flush_stride;
  uint8_t duty_offset;
  uint8_t duty_width;
  uint8_t day_alarm;
  uint8_t month_alarm;
  uint8_t century;
} __attribute__((packed));

void init();
void poweroff();
void reboot();

} // namespace drivers::acpi
