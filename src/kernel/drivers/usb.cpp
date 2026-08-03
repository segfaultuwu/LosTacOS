#include "LTOS/drivers/usb.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/uhci.hpp"
#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include <string.h>

namespace drivers::usb {

static HidDevice hid_kbd;
static bool has_kbd = false;
static bool initialized = false;

static uint8_t hid_to_ps2(uint8_t hid_code) {
  switch (hid_code) {
  case 0x04: return 0x1E;
  case 0x05: return 0x1F;
  case 0x06: return 0x20;
  case 0x07: return 0x21;
  case 0x08: return 0x22;
  case 0x09: return 0x23;
  case 0x0A: return 0x24;
  case 0x0B: return 0x25;
  case 0x0C: return 0x26;
  case 0x0D: return 0x27;
  case 0x0E: return 0x28;
  case 0x0F: return 0x29;
  case 0x10: return 0x2A;
  case 0x11: return 0x2B;
  case 0x12: return 0x2C;
  case 0x13: return 0x2D;
  case 0x14: return 0x2E;
  case 0x15: return 0x2F;
  case 0x16: return 0x30;
  case 0x17: return 0x31;
  case 0x18: return 0x32;
  case 0x19: return 0x33;
  case 0x1A: return 0x34;
  case 0x1B: return 0x35;
  case 0x1C: return 0x36;
  case 0x1D: return 0x37;
  case 0x1E: return 0x01;
  case 0x1F: return 0x03;
  case 0x20: return 0x05;
  case 0x21: return 0x07;
  case 0x22: return 0x09;
  case 0x23: return 0x0B;
  case 0x24: return 0x0D;
  case 0x25: return 0x0F;
  case 0x26: return 0x11;
  case 0x27: return 0x13;
  case 0x28: return 0x1C;
  case 0x29: return 0x01;
  case 0x2A: return 0x0E;
  case 0x2B: return 0x0F;
  case 0x2C: return 0x2C;
  case 0x2D: return 0x2D;
  case 0x2E: return 0x01;
  case 0x2F: return 0x10;
  case 0x30: return 0x11;
  case 0x31: return 0x12;
  case 0x32: return 0x13;
  case 0x33: return 0x17;
  case 0x34: return 0x18;
  case 0x35: return 0x19;
  case 0x36: return 0x15;
  case 0x37: return 0x2E;
  case 0x38: return 0x1A;
  case 0x39: return 0x3A;
  case 0x3A: return 0x3E;
  case 0x3B: return 0x3C;
  case 0x3C: return 0x3D;
  case 0x3D: return 0x3F;
  case 0x3E: return 0x41;
  case 0x3F: return 0x42;
  case 0x40: return 0x43;
  case 0x41: return 0x44;
  case 0x42: return 0x45;
  case 0x43: return 0x46;
  case 0x44: return 0x57;
  case 0x45: return 0x58;
  case 0x46: return 0x37;
  case 0x47: return 0x48;
  case 0x48: return 0x49;
  case 0x49: return 0x4A;
  case 0x4A: return 0x4B;
  case 0x4B: return 0x51;
  case 0x4C: return 0x4D;
  case 0x4D: return 0x4E;
  case 0x4E: return 0x4C;
  case 0x4F: return 0x50;
  case 0x50: return 0x4F;
  case 0x51: return 0x52;
  case 0x52: return 0x53;
  case 0x53: return 0x47;
  case 0x54: return 0x4B;
  case 0x55: return 0x4C;
  case 0x56: return 0x4D;
  case 0x57: return 0x47;
  case 0x58: return 0x49;
  case 0x59: return 0x51;
  case 0x5A: return 0x4F;
  case 0x5B: return 0x50;
  case 0x5C: return 0x51;
  case 0x5D: return 0x52;
  case 0x5E: return 0x4F;
  case 0x5F: return 0x50;
  case 0x60: return 0x51;
  case 0x61: return 0x52;
  case 0x62: return 0x53;
  case 0x63: return 0x4A;
  case 0x64: return 0x56;
  case 0x65: return 0x5B;
  case 0x87: return 0x47;
  case 0x89: return 0x4F;
  case 0x8A: return 0x50;
  case 0x8B: return 0x51;
  case 0x8C: return 0x52;
  case 0x8D: return 0x53;
  default: return 0;
  }
}

static int usb_control_in(uint8_t dev_addr, uint8_t req_type, uint8_t req, uint16_t value,
                          uint16_t index, void *buf, size_t len) {
  return uhci::control_in(dev_addr, 0, req_type, req, value, index, buf, len);
}

static int usb_control_out(uint8_t dev_addr, uint8_t req_type, uint8_t req, uint16_t value,
                           uint16_t index, const void *buf, size_t len) {
  return uhci::control_out(dev_addr, 0, req_type, req, value, index, buf, len);
}

static int interrupt_in(uint8_t dev_addr, uint8_t ep, void *buf, size_t len) {
  static bool toggle = false;
  int ret = uhci::int_in(dev_addr, ep & 0xF, toggle, false, buf, len);
  if (ret > 0)
    toggle = !toggle;
  return ret;
}

static bool enumerate_hid_keyboard() {
  DeviceDescriptor dev_desc;
  int ret = usb_control_in(0, BM_REQ_DEV_TO_HOST | BM_REQ_STANDARD | BM_REQ_RECIPIENT_DEVICE,
                           REQ_GET_DESCRIPTOR, DESC_DEVICE << 8, 0, &dev_desc, sizeof(dev_desc));
  if (ret < 0)
    return false;

  logger::info("[USB] Device: vid=%04x pid=%04x class=%02x", dev_desc.vendor_id,
               dev_desc.product_id, dev_desc.dev_class);

  uint8_t address = 1;

  ret = usb_control_out(0, BM_REQ_HOST_TO_DEV | BM_REQ_STANDARD | BM_REQ_RECIPIENT_DEVICE,
                        REQ_SET_ADDRESS, address, 0, nullptr, 0);
  if (ret < 0)
    return false;

  timer::delay_ms(10);

  uint8_t config_buf[64];
  ret = usb_control_in(address, BM_REQ_DEV_TO_HOST | BM_REQ_STANDARD | BM_REQ_RECIPIENT_DEVICE,
                       REQ_GET_DESCRIPTOR, DESC_CONFIG << 8, 0, config_buf, 64);
  if (ret < 0)
    return false;

  ConfigDescriptor *cfg = (ConfigDescriptor *)config_buf;
  InterfaceDescriptor *iface = nullptr;
  EndpointDescriptor *ep = nullptr;

  uint8_t *ptr = config_buf + cfg->length;
  uint8_t *end = config_buf + cfg->total_length;

  while (ptr < end) {
    uint8_t *desc = ptr;
    uint8_t desc_len = desc[0];
    uint8_t desc_type = desc[1];

    if (desc_type == 4) {
      iface = (InterfaceDescriptor *)desc;
    } else if (desc_type == 5) {
      ep = (EndpointDescriptor *)desc;
    } else if (desc_type == 0x21) {
      HidDescriptor *hid = (HidDescriptor *)desc;
      (void)hid;
    }

    ptr += desc_len;
    if (desc_len == 0)
      break;
  }

  if (!iface || iface->iface_class != 3 || iface->iface_subclass != 1 ||
      iface->iface_protocol != 1) {
    logger::info("[USB] Not a HID boot keyboard (class=%02x subclass=%02x protocol=%02x)",
                 iface ? iface->iface_class : 0, iface ? iface->iface_subclass : 0,
                 iface ? iface->iface_protocol : 0);
    return false;
  }

  if (!ep || (ep->ep_address & 0x80) == 0) {
    logger::warn("[USB] No IN endpoint for HID keyboard");
    return false;
  }

  ret = usb_control_out(address, BM_REQ_HOST_TO_DEV | BM_REQ_STANDARD | BM_REQ_RECIPIENT_DEVICE,
                        REQ_SET_CONFIGURATION, cfg->config_value, 0, nullptr, 0);
  if (ret < 0)
    return false;

  timer::delay_ms(10);

  ret = usb_control_out(address, BM_REQ_HOST_TO_DEV | BM_REQ_CLASS | BM_REQ_RECIPIENT_INTERFACE,
                        REQ_SET_PROTOCOL, HID_BOOT_PROTOCOL, 0, nullptr, 0);
  if (ret < 0)
    return false;

  ret = usb_control_out(address, BM_REQ_HOST_TO_DEV | BM_REQ_CLASS | BM_REQ_RECIPIENT_INTERFACE,
                        REQ_SET_IDLE, 0, 0, nullptr, 0);
  if (ret < 0)
    return false;

  hid_kbd.address = (uint8_t)(ep->ep_address & 0x7F);
  hid_kbd.ep_in = ep->ep_address & 0x8F;
  hid_kbd.ep_max_packet = ep->max_packet_size;
  hid_kbd.vendor_id = dev_desc.vendor_id;
  hid_kbd.product_id = dev_desc.product_id;
  has_kbd = true;

  logger::info("[USB] HID keyboard on addr %d ep=0x%02x max_pkt=%d", hid_kbd.address,
               hid_kbd.ep_in, hid_kbd.ep_max_packet);

  return true;
}

bool init() {
  if (initialized)
    return has_kbd;

  if (!uhci::can_transfer())
    return false;

  if (enumerate_hid_keyboard()) {
    initialized = true;
    return true;
  }

  return false;
}

bool device_available() {
  return has_kbd && uhci::can_transfer();
}

int keyboard_poll(uint8_t *scancode) {
  if (!has_kbd)
    return -1;

  uint8_t report[8];
  int ret = interrupt_in(hid_kbd.address, hid_kbd.ep_in, report, sizeof(report));
  if (ret <= 0)
    return 0;

  static uint8_t prev[8] = {0};
  bool changed = false;

  for (int i = 0; i < 8; i++) {
    if (report[i] != prev[i]) {
      changed = true;
      break;
    }
  }

  if (!changed) {
    memcpy(prev, report, 8);
    return 0;
  }

  uint8_t mod_change = report[0] ^ prev[0];

  for (int bit = 0; bit < 8; bit++) {
    if (mod_change & (1 << bit)) {
      bool pressed = (report[0] & (1 << bit)) != 0;
      switch (bit) {
      case 0:
        *scancode = pressed ? 0x1D : 0x9D;
        return 1;
      case 1:
        *scancode = pressed ? 0x2A : 0xAA;
        return 1;
      case 2:
        *scancode = pressed ? 0x38 : 0xB8;
        return 1;
      }
    }
  }

  for (int k = 2; k < 8; k++) {
    if (report[k] == 0)
      continue;

    bool was_held = false;
    for (int p = 2; p < 8; p++) {
      if (prev[p] == report[k]) {
        was_held = true;
        break;
      }
    }

    if (!was_held) {
      uint8_t sc = hid_to_ps2(report[k]);
      *scancode = sc;
      memcpy(prev, report, 8);
      return 1;
    }
  }

  for (int k = 2; k < 8; k++) {
    if (prev[k] == 0)
      continue;

    bool still_held = false;
    for (int p = 2; p < 8; p++) {
      if (report[p] == prev[k]) {
        still_held = true;
        break;
      }
    }

    if (!still_held) {
      uint8_t sc = hid_to_ps2(prev[k]);
      *scancode = sc | 0x80;
      memcpy(prev, report, 8);
      return 1;
    }
  }

  memcpy(prev, report, 8);
  return 0;
}

} // namespace drivers::usb
