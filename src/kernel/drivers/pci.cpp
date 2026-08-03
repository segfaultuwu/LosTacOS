#include "LTOS/drivers/pci.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/logger.hpp"

namespace drivers::pci {

constexpr uint16_t PCI_CONFIG_ADDRESS = 0xCF8;
constexpr uint16_t PCI_CONFIG_DATA = 0xCFC;

using drivers::serial::outl;
using drivers::serial::inl;

uint32_t read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) |
                                ((uint32_t)0x80000000));
  outl(PCI_CONFIG_ADDRESS, address);
  return inl(PCI_CONFIG_DATA);
}

uint16_t read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t val = read_config32(bus, slot, func, offset);
  return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t val = read_config32(bus, slot, func, offset);
  return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void write_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
  uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) |
                                ((uint32_t)0x80000000));
  outl(PCI_CONFIG_ADDRESS, address);
  outl(PCI_CONFIG_DATA, val);
}

void write_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
  uint32_t orig = read_config32(bus, slot, func, offset);
  int shift = (offset & 2) * 8;
  orig &= ~(0xFFFF << shift);
  orig |= ((uint32_t)val << shift);
  write_config32(bus, slot, func, offset, orig);
}

void enable_bus_mastering(const PciDevice *dev) {
  // Read-modify-write the command register; collapsing the read+write
  // into 32-bit ops saves one PCI config cycle per device.
  uint32_t cmd = read_config32(dev->bus, dev->slot, dev->func, 0x04);
  cmd |= 0x07; // Memory Space | I/O Space | Bus Master
  write_config32(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

static void fill_device_info(uint8_t bus, uint8_t slot, uint8_t func, PciDevice *dev) {
  dev->bus = bus;
  dev->slot = slot;
  dev->func = func;
  dev->vendor_id = read_config16(bus, slot, func, 0x00);
  dev->device_id = read_config16(bus, slot, func, 0x02);
  dev->class_code = read_config8(bus, slot, func, 0x0B);
  dev->subclass = read_config8(bus, slot, func, 0x0A);
  dev->prog_if = read_config8(bus, slot, func, 0x09);
  dev->header_type = read_config8(bus, slot, func, 0x0E);

  for (int i = 0; i < 6; i++) {
    dev->bar[i] = read_config32(bus, slot, func, 0x10 + (i * 4));
  }
}

bool find_device_by_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, PciDevice *out) {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        // Vendor + Device ID are packed in a single dword at 0x00 -- one
        // config read instead of two. Same trick at 0x08 which carries
        // (revision, prog_if, subclass, class_code) all in one dword, so
        // the empty-slot fast path (vendor == 0xFFFF) is now a single
        // 32-bit PCI config read instead of five 8/16-bit ones. On a
        // mostly-empty bus this turns the scan from ~65000us into
        // ~65000 / 5 = ~13ms.
        uint32_t id = read_config32((uint8_t)bus, slot, func, 0x00);
        if ((id & 0xFFFF) == 0xFFFF)
          continue;

        uint32_t class_dword = read_config32((uint8_t)bus, slot, func, 0x08);
        uint8_t pi = (uint8_t)(class_dword >> 8);
        uint8_t sc = (uint8_t)(class_dword >> 16);
        uint8_t cc = (uint8_t)(class_dword >> 24);

        if (cc == class_code && sc == subclass && (prog_if == 0xFF || pi == prog_if)) {
          fill_device_info((uint8_t)bus, slot, func, out);
          return true;
        }

        // Only need the multi-function bit when func 0 actually has a
        // device behind it -- reading header_type for empty func 0
        // slots (the common case) was a wasted read.
        if (func == 0) {
          uint8_t header = read_config8((uint8_t)bus, slot, func, 0x0E);
          if (!(header & 0x80))
            break;
        }
      }
    }
  }
  return false;
}

bool find_device_by_id(uint16_t vendor_id, uint16_t device_id, PciDevice *out) {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        // vendor + device ID live in the same dword at 0x00.
        uint32_t id = read_config32((uint8_t)bus, slot, func, 0x00);
        if ((id & 0xFFFF) == 0xFFFF)
          continue;

        uint16_t vendor = (uint16_t)(id & 0xFFFF);
        uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);
        if (vendor == vendor_id && device == device_id) {
          fill_device_info((uint8_t)bus, slot, func, out);
          return true;
        }

        if (func == 0) {
          uint8_t header = read_config8((uint8_t)bus, slot, func, 0x0E);
          if (!(header & 0x80))
            break;
        }
      }
    }
  }
  return false;
}

void init() {
  logger::info("PCI bus initialized");
}

} // namespace drivers::pci
