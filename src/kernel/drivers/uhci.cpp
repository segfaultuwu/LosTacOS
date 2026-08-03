#include "LTOS/drivers/uhci.hpp"
#include "LTOS/drivers/pci.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/paging.hpp"
#include <string.h>

namespace drivers::uhci {

constexpr uint16_t UHCI_USBCMD = 0x00;
constexpr uint16_t UHCI_USBSTS = 0x02;
constexpr uint16_t UHCI_USBINTR = 0x04;
constexpr uint16_t UHCI_FRNUM = 0x06;
constexpr uint16_t UHCI_FRBASEADD = 0x08;
constexpr uint16_t UHCI_SOFMOD = 0x0C;
constexpr uint16_t UHCI_PORTSC1 = 0x10;
constexpr uint16_t UHCI_PORTSC2 = 0x12;

constexpr uint16_t CMD_RUN = 0x0001;
constexpr uint16_t CMD_HCRESET = 0x0002;
constexpr uint16_t CMD_GRESET = 0x0004;
constexpr uint16_t CMD_CONFIGURE = 0x0040;

constexpr uint16_t STS_USBINT = 0x0001;
constexpr uint16_t STS_ERRINT = 0x0002;
constexpr uint16_t STS_RD = 0x0020;
constexpr uint16_t STS_HCHALTED = 0x0040;
constexpr uint16_t STS_HCPROCESS = 0x0080;

constexpr uint16_t PORT_CONNECT = 0x0001;
constexpr uint16_t PORT_ENABLE = 0x0004;
constexpr uint16_t PORT_SUSP = 0x0008;
constexpr uint16_t PORT_RESET = 0x0200;
constexpr uint16_t PORT_LS = 0x0100;
constexpr uint16_t PORT_RD = 0x0040;
constexpr uint16_t PORT_WRITE = 0x0010;
constexpr uint16_t PORT_CONNECT_CHG = 0x0002;
constexpr uint16_t PORT_ENABLE_CHG = 0x0000;

constexpr uint8_t TD_PID_SETUP = 0x2D;
constexpr uint8_t TD_PID_IN = 0x69;
constexpr uint8_t TD_PID_OUT = 0xE1;

constexpr uint32_t TD_ACTIVE = 1 << 23;
constexpr uint32_t TD_IOC = 1 << 24;
constexpr uint32_t TD_LS = 1 << 26;
constexpr uint32_t TD_SPD = 1 << 29;
constexpr uint32_t TD_NAK = 1 << 19;
constexpr uint32_t TD_STALLED = 1 << 22;
constexpr uint32_t TD_DBUF = 1 << 22;
constexpr uint32_t TD_BABBLE = 1 << 20;
constexpr uint32_t TD_CRC = 1 << 22;

constexpr uint32_t TD_ERROR = TD_STALLED | TD_DBUF | TD_BABBLE | TD_CRC;

struct Td {
  volatile uint32_t link;
  volatile uint32_t status;
  volatile uint32_t token;
  volatile uint32_t buffer;
};

struct Qh {
  volatile uint32_t head;
  volatile uint32_t element;
  uint32_t reserved[2];
};

static uint16_t io_base = 0;
static uint16_t port_count = 2;
static Td *td_pool = nullptr;
static Qh *async_qh = nullptr;
static uint32_t *frame_list = nullptr;
static int td_pool_index = 0;
static bool controller_ready = false;

using drivers::serial::outw;
using drivers::serial::inw;

static inline uint32_t to_phys(void *p) {
  return (uint32_t)(uint64_t)p;
}

static void wait_for_port_reset(uint16_t port_reg) {
  auto start = inw(io_base + port_reg);
  outw(io_base + port_reg, start | PORT_RESET);
  for (volatile int i = 0; i < 100000; i++)
    asm volatile("pause");
  outw(io_base + port_reg, start & ~(uint16_t)PORT_RESET);
  for (volatile int i = 0; i < 100000; i++)
    asm volatile("pause");
}

static Td *alloc_td() {
  if (td_pool_index >= 128)
    return nullptr;
  Td *td = &td_pool[td_pool_index++];
  memset((void *)td, 0, sizeof(Td));
  return td;
}

static void free_all_tds() {
  td_pool_index = 0;
}

static void td_set_link(Td *td, Td *next, bool qh, bool depth) {
  td->link = to_phys(next);
  if (qh)
    td->link |= 1 << 1;
  if (depth)
    td->link |= 1 << 2;
}

static void td_set_terminate(Td *td) {
  td->link = 1;
}

static void td_set_status(Td *td, uint32_t maxlen, bool is_ls, bool ioc) {
  td->status = maxlen - 1;
  td->status |= TD_ACTIVE;
  if (is_ls)
    td->status |= TD_LS;
  if (ioc)
    td->status |= TD_IOC;
}

static void td_set_token(Td *td, uint8_t pid, uint8_t dev_addr, uint8_t ep, bool data_toggle) {
  td->token = (ep & 0xF) << 15;
  td->token |= (dev_addr & 0x7F) << 8;
  td->token |= pid;
  if (data_toggle)
    td->token |= 1 << 19;
}

static void td_set_buffer(Td *td, void *buf) {
  td->buffer = to_phys(buf);
}

static bool td_error(Td *td) {
  uint32_t s = td->status;
  return (s & TD_ACTIVE) == 0 && (s & (TD_STALLED | TD_CRC | TD_BABBLE | TD_DBUF)) != 0;
}

static int td_actual_length(Td *td) {
  return (td->status + 1) & 0x7FF;
}

static bool wait_td(Td *td) {
  uint32_t timeout = 100000;
  while (td->status & TD_ACTIVE) {
    if (timeout-- == 0)
      return false;
    asm volatile("pause");
  }
  return !td_error(td);
}

int control_in(uint8_t dev_addr, uint8_t ep, uint8_t bm_req_type, uint8_t req, uint16_t value,
               uint16_t index, void *buf, size_t len) {
  if (!controller_ready || !td_pool)
    return -1;

  free_all_tds();

  uint8_t setup_packet[8];
  setup_packet[0] = bm_req_type;
  setup_packet[1] = req;
  setup_packet[2] = value & 0xFF;
  setup_packet[3] = (value >> 8) & 0xFF;
  setup_packet[4] = index & 0xFF;
  setup_packet[5] = (index >> 8) & 0xFF;
  setup_packet[6] = len & 0xFF;
  setup_packet[7] = (len >> 8) & 0xFF;

  bool is_ls = false;

  Td *setup_td = alloc_td();
  Td *status_td = alloc_td();

  if (!setup_td || !status_td)
    return -1;

  Td *data_td = nullptr;

  if (len > 0) {
    data_td = alloc_td();
    if (!data_td)
      return -1;

    td_set_link(setup_td, data_td, false, true);
    td_set_link(data_td, status_td, false, true);
    td_set_terminate(status_td);

    td_set_status(setup_td, 8, is_ls, false);
    td_set_token(setup_td, TD_PID_SETUP, dev_addr, ep, false);
    td_set_buffer(setup_td, setup_packet);

    td_set_status(data_td, len, is_ls, false);
    td_set_token(data_td, TD_PID_IN, dev_addr, ep, true);
    td_set_buffer(data_td, buf);

    td_set_status(status_td, 0, is_ls, true);
    td_set_token(status_td, TD_PID_OUT, dev_addr, ep, true);
    td_set_buffer(status_td, nullptr);
  } else {
    td_set_link(setup_td, status_td, false, true);
    td_set_terminate(status_td);

    td_set_status(setup_td, 8, is_ls, false);
    td_set_token(setup_td, TD_PID_SETUP, dev_addr, ep, false);
    td_set_buffer(setup_td, setup_packet);

    td_set_status(status_td, 0, is_ls, true);
    td_set_token(status_td, TD_PID_IN, dev_addr, ep, true);
    td_set_buffer(status_td, nullptr);
  }

  async_qh->element = to_phys(setup_td);
  async_qh->head = to_phys(setup_td);

  timer::delay_ms(1);

  if (!wait_td(status_td))
    return -1;

  if (data_td)
    return td_actual_length(data_td);

  return 0;
}

int control_out(uint8_t dev_addr, uint8_t ep, uint8_t bm_req_type, uint8_t req, uint16_t value,
                uint16_t index, const void *buf, size_t len) {
  if (!controller_ready || !td_pool)
    return -1;

  free_all_tds();

  uint8_t setup_packet[8];
  setup_packet[0] = bm_req_type;
  setup_packet[1] = req;
  setup_packet[2] = value & 0xFF;
  setup_packet[3] = (value >> 8) & 0xFF;
  setup_packet[4] = index & 0xFF;
  setup_packet[5] = (index >> 8) & 0xFF;
  setup_packet[6] = len & 0xFF;
  setup_packet[7] = (len >> 8) & 0xFF;

  Td *setup_td = alloc_td();
  Td *status_td = alloc_td();

  if (!setup_td || !status_td)
    return -1;

  if (len > 0) {
    Td *data_td = alloc_td();
    if (!data_td)
      return -1;

    td_set_link(setup_td, data_td, false, true);
    td_set_link(data_td, status_td, false, true);
    td_set_terminate(status_td);

    td_set_status(setup_td, 8, false, false);
    td_set_token(setup_td, TD_PID_SETUP, dev_addr, ep, false);
    td_set_buffer(setup_td, setup_packet);

    td_set_status(data_td, len, false, true);
    td_set_token(data_td, TD_PID_OUT, dev_addr, ep, true);
    td_set_buffer(data_td, (void *)buf);

    td_set_status(status_td, 0, false, false);
    td_set_token(status_td, TD_PID_IN, dev_addr, ep, true);
    td_set_buffer(status_td, nullptr);
  } else {
    td_set_link(setup_td, status_td, false, true);
    td_set_terminate(status_td);

    td_set_status(setup_td, 8, false, false);
    td_set_token(setup_td, TD_PID_SETUP, dev_addr, ep, false);
    td_set_buffer(setup_td, setup_packet);

    td_set_status(status_td, 0, false, true);
    td_set_token(status_td, TD_PID_IN, dev_addr, ep, true);
    td_set_buffer(status_td, nullptr);
  }

  async_qh->element = to_phys(setup_td);
  async_qh->head = to_phys(setup_td);

  timer::delay_ms(1);

  if (!wait_td(status_td))
    return -1;

  return len;
}

int int_in(uint8_t dev_addr, uint8_t ep, bool data_toggle, bool is_ls, void *buf, size_t len) {
  if (!controller_ready || !td_pool)
    return -1;

  free_all_tds();

  Td *td = alloc_td();
  if (!td)
    return -1;

  td_set_terminate(td);
  td_set_status(td, len, is_ls, true);
  td_set_token(td, TD_PID_IN, dev_addr, ep & 0xF, data_toggle);
  td_set_buffer(td, buf);

  async_qh->element = to_phys(td);

  timer::delay_ms(1);

  if (!wait_td(td))
    return -1;

  return td_actual_length(td);
}

bool can_transfer() {
  return controller_ready;
}

bool init() {
  pci::PciDevice dev;
  if (!pci::find_device_by_class(0x0C, 0x03, 0x00, &dev)) {
    logger::warn("[UHCI] No UHCI controller found");
    return false;
  }

  uint32_t bar4 = dev.bar[4];
  if (!bar4 || (bar4 & 1) == 0) {
    logger::warn("[UHCI] BAR4 is not I/O space");
    return false;
  }

  io_base = bar4 & ~3;
  logger::info("[UHCI] Found at %02x:%02x.%d IO=%x", dev.bus, dev.slot, dev.func, io_base);

  pci::enable_bus_mastering(&dev);

  outw(io_base + UHCI_USBCMD, 0);
  for (volatile int i = 0; i < 10000; i++)
    asm volatile("pause");

  uint16_t cmd = inw(io_base + UHCI_USBCMD);
  if (cmd & CMD_HCRESET) {
    logger::warn("[UHCI] Controller did not come out of reset");
    return false;
  }

  outw(io_base + UHCI_USBCMD, CMD_GRESET);
  for (volatile int i = 0; i < 100000; i++)
    asm volatile("pause");
  outw(io_base + UHCI_USBCMD, 0);

  outw(io_base + UHCI_USBINTR, 0);
  outw(io_base + UHCI_USBSTS, 0x3F);

  frame_list = (uint32_t *)paging::alloc_page();
  if (!frame_list)
    return false;
  memset(frame_list, 0, 4096);

  async_qh = (Qh *)paging::alloc_page();
  if (!async_qh)
    return false;
  memset(async_qh, 0, 4096);

  td_pool = (Td *)paging::alloc_page();
  if (!td_pool)
    return false;
  memset(td_pool, 0, 4096);
  td_pool_index = 0;

  async_qh->head = 1;
  async_qh->element = 1;

  for (int i = 0; i < 1024; i++)
    frame_list[i] = to_phys(async_qh) | 1 << 1;

  outw(io_base + UHCI_FRBASEADD, (uint16_t)(to_phys(frame_list) & 0xFFFF));

  outw(io_base + UHCI_SOFMOD, 64);

  port_count = 0;
  for (int p = 0; p < 2; p++) {
    uint16_t port_reg = (p == 0) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    uint16_t portsc = inw(io_base + port_reg);
    if (portsc != 0xFFFF) {
      port_count++;
    }
  }

  outw(io_base + UHCI_USBCMD, CMD_RUN | CMD_CONFIGURE);
  for (volatile int i = 0; i < 10000; i++)
    asm volatile("pause");

  uint16_t sts = inw(io_base + UHCI_USBSTS);
  if (sts & STS_HCHALTED) {
    logger::warn("[UHCI] Controller halted after start (sts=%x)", sts);
    return false;
  }

  for (int p = 0; p < port_count; p++) {
    uint16_t port_reg = (p == 0) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    uint16_t portsc = inw(io_base + port_reg);

    if (!(portsc & PORT_CONNECT)) {
      logger::info("[UHCI] Port %d: no device", p + 1);
      continue;
    }

    wait_for_port_reset(port_reg);

    timer::delay_ms(10);

    portsc = inw(io_base + port_reg);
    bool is_ls = (portsc & PORT_LS) != 0;

    logger::info("[UHCI] Port %d: device connected (%s speed)", p + 1, is_ls ? "low" : "full");

    if (!(portsc & PORT_ENABLE)) {
      outw(io_base + port_reg, portsc | PORT_ENABLE);
      timer::delay_ms(10);
    }
  }

  controller_ready = true;
  logger::info("[UHCI] Initialized with %d ports", port_count);
  return true;
}

} // namespace drivers::uhci
