#pragma once

#include <stdint.h>

namespace drivers::usb {

struct DeviceDescriptor {
  uint8_t length;
  uint8_t type;
  uint16_t usb_version;
  uint8_t dev_class;
  uint8_t dev_subclass;
  uint8_t dev_protocol;
  uint8_t max_packet_size;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t device_version;
  uint8_t manufacturer_index;
  uint8_t product_index;
  uint8_t serial_index;
  uint8_t num_configurations;
} __attribute__((packed));

struct ConfigDescriptor {
  uint8_t length;
  uint8_t type;
  uint16_t total_length;
  uint8_t num_interfaces;
  uint8_t config_value;
  uint8_t config_index;
  uint8_t attributes;
  uint8_t max_power;
} __attribute__((packed));

struct InterfaceDescriptor {
  uint8_t length;
  uint8_t type;
  uint8_t interface_num;
  uint8_t alt_setting;
  uint8_t num_endpoints;
  uint8_t iface_class;
  uint8_t iface_subclass;
  uint8_t iface_protocol;
  uint8_t iface_index;
} __attribute__((packed));

struct EndpointDescriptor {
  uint8_t length;
  uint8_t type;
  uint8_t ep_address;
  uint8_t attributes;
  uint16_t max_packet_size;
  uint8_t interval;
} __attribute__((packed));

struct HidDescriptor {
  uint8_t length;
  uint8_t type;
  uint16_t hid_version;
  uint8_t country_code;
  uint8_t num_descriptors;
  uint8_t desc_type;
  uint16_t desc_length;
} __attribute__((packed));

struct HidDevice {
  uint8_t address;
  uint8_t ep_in;
  uint16_t ep_max_packet;
  uint16_t vendor_id;
  uint16_t product_id;
};

constexpr uint8_t REQ_GET_DESCRIPTOR = 6;
constexpr uint8_t REQ_SET_ADDRESS = 5;
constexpr uint8_t REQ_SET_CONFIGURATION = 9;
constexpr uint8_t REQ_SET_IDLE = 0x0A;
constexpr uint8_t REQ_SET_PROTOCOL = 0x0B;

constexpr uint8_t DESC_DEVICE = 1;
constexpr uint8_t DESC_CONFIG = 2;
constexpr uint8_t DESC_HID = 0x21;
constexpr uint8_t DESC_REPORT = 0x22;

constexpr uint8_t BM_REQ_DEV_TO_HOST = 0x80;
constexpr uint8_t BM_REQ_HOST_TO_DEV = 0x00;
constexpr uint8_t BM_REQ_STANDARD = 0x00;
constexpr uint8_t BM_REQ_CLASS = 0x20;
constexpr uint8_t BM_REQ_RECIPIENT_DEVICE = 0x00;
constexpr uint8_t BM_REQ_RECIPIENT_INTERFACE = 0x01;

constexpr uint8_t HID_BOOT_PROTOCOL = 0;
constexpr uint8_t HID_REPORT_PROTOCOL = 1;

bool init();
bool device_available();
int keyboard_poll(uint8_t *scancode);

} // namespace drivers::usb
