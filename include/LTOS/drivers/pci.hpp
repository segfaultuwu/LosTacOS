#pragma once

#include <stdint.h>
#include <stddef.h>

namespace drivers::pci {

struct PciDevice {
  uint8_t bus;
  uint8_t slot;
  uint8_t func;
  uint16_t vendor_id;
  uint16_t device_id;
  uint8_t class_code;
  uint8_t subclass;
  uint8_t prog_if;
  uint8_t header_type;
  uint32_t bar[6];
};

void init();

uint32_t read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

void write_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void write_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

bool find_device_by_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, PciDevice *out);
bool find_device_by_id(uint16_t vendor_id, uint16_t device_id, PciDevice *out);

void enable_bus_mastering(const PciDevice *dev);

} // namespace drivers::pci
