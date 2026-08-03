#include "LTOS/drivers/ahci.hpp"
#include "LTOS/drivers/pci.hpp"
#include "LTOS/fs/devfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/paging.hpp"

#include <string.h>

namespace drivers::ahci {

constexpr uint32_t SATA_SIG_ATA = 0x00000101;
constexpr uint32_t SATA_SIG_ATAPI = 0xEB140101;

constexpr uint8_t ATA_CMD_READ_DMA_EXT = 0x25;
constexpr uint8_t ATA_CMD_WRITE_DMA_EXT = 0x35;
constexpr uint8_t ATA_CMD_IDENTIFY = 0xEC;
constexpr uint8_t ATA_CMD_IDENTIFY_PACKET = 0xA1;
constexpr uint8_t ATA_CMD_PACKET = 0xA0;

constexpr uint32_t HBA_PxCMD_ST = 1 << 0;
constexpr uint32_t HBA_PxCMD_FRE = 1 << 4;

constexpr uint32_t HBA_PxCMD_FR = 1 << 14;
constexpr uint32_t HBA_PxCMD_CR = 1 << 15;

constexpr uint32_t TFD_STS_ERR = 0x01;
constexpr uint32_t TFD_STS_DRQ = 0x08;
constexpr uint32_t TFD_STS_BSY = 0x80;

struct HbaPort {

  volatile uint32_t clb;
  volatile uint32_t clbu;

  volatile uint32_t fb;
  volatile uint32_t fbu;

  volatile uint32_t is;
  volatile uint32_t ie;

  volatile uint32_t cmd;

  uint32_t reserved0;

  volatile uint32_t tfd;
  volatile uint32_t sig;

  volatile uint32_t ssts;
  volatile uint32_t sctl;
  volatile uint32_t serr;

  volatile uint32_t sact;
  volatile uint32_t ci;

  volatile uint32_t sntf;
  volatile uint32_t fbs;

  uint32_t reserved1[11];

  uint32_t vendor[4];
};

constexpr uint32_t CAP_NCS = 0x1F00;
constexpr uint32_t CAP_NCS_SHIFT = 8;
constexpr uint32_t CAP_NP_MASK = 0x1F;

struct HbaMem {
  volatile uint32_t cap;
  volatile uint32_t ghc;
  volatile uint32_t is;
  volatile uint32_t pi;
  volatile uint32_t vs;
  volatile uint32_t ccc_ctl;
  volatile uint32_t ccc_pts;
  volatile uint32_t em_loc;
  volatile uint32_t em_ctl;
  volatile uint32_t cap2;
  volatile uint32_t bohc;
  uint32_t reserved[51];
  HbaPort ports[32];
};

struct AhciController {
  HbaMem *abar;
  uint8_t bus;
  uint8_t slot;
  uint8_t func;
};

struct HbaCmdHeader {

  uint8_t cfl : 5;
  uint8_t a : 1;
  uint8_t w : 1;
  uint8_t p : 1;

  uint8_t r : 1;
  uint8_t b : 1;
  uint8_t c : 1;
  uint8_t rsv : 1;
  uint8_t pmp : 4;

  uint16_t prdtl;

  uint32_t prdbc;

  uint32_t ctba;
  uint32_t ctbau;

  uint32_t reserved[4];
};

struct HbaPrdtEntry {

  uint32_t dba;
  uint32_t dbau;

  uint32_t reserved;

  uint32_t dbc : 22;
  uint32_t reserved2 : 9;
  uint32_t i : 1;
};

struct HbaCmdTable {

  uint8_t cfis[64];

  uint8_t acmd[16];

  uint8_t reserved[48];

  HbaPrdtEntry prdt[1];
};

struct FisRegH2D {

  uint8_t type;

  uint8_t pmport : 4;
  uint8_t reserved : 3;
  uint8_t c : 1;

  uint8_t command;
  uint8_t feature;

  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;

  uint8_t device;

  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;

  uint8_t feature_high;

  uint8_t count_low;
  uint8_t count_high;

  uint8_t icc;
  uint8_t control;

  uint8_t reserved2[4];
};

static void ident_swap(char *dst, const uint16_t *src, int words) {
  for (int i = 0; i < words; i++) {
    dst[i * 2] = (src[i] >> 8) & 0xFF;
    dst[i * 2 + 1] = src[i] & 0xFF;
  }
}

struct AhciPortInfo {
  uint8_t number;
  HbaPort *port;
  uint64_t sectors;
  bool active;
  bool is_atapi;
  char model[41];
  char serial[21];
  HbaCmdHeader *cmd_list;
  HbaCmdTable *cmd_table;
  void *fis;
  void *buffer;
};

static AhciController controllers[4];
static int controller_count = 0;

static AhciPortInfo ports[32];

static bool initialized = false;

bool is_available() {
  return initialized;
}

uint64_t get_capacity_sectors(uint8_t port) {
  if (port >= 32)
    return 0;

  if (!ports[port].active)
    return 0;

  return ports[port].sectors;
}

static void stop_cmd(HbaPort *p) {

  p->cmd &= ~HBA_PxCMD_ST;
  p->cmd &= ~HBA_PxCMD_FRE;

  while (p->cmd & (HBA_PxCMD_FR | HBA_PxCMD_CR))
    ;
}

static void start_cmd(HbaPort *p) {

  while (p->cmd & HBA_PxCMD_CR)
    ;

  p->cmd |= HBA_PxCMD_FRE;

  p->cmd |= HBA_PxCMD_ST;
}

static int find_slot(HbaPort *p) {

  uint32_t slots = p->sact | p->ci;

  for (int i = 0; i < 32; i++) {
    if (!(slots & (1 << i)))
      return i;
  }

  return -1;
}

static bool wait(HbaPort *p, int slot) {
  uint32_t timeout = 1000000;

  while (timeout--) {
    if (!(p->ci & (1 << slot))) {
      if (p->tfd & TFD_STS_ERR)
        return false;
      return true;
    }

    if (p->is & (1 << 30))
      return false;
  }

  return false;
}

static bool setup_command(AhciPortInfo *info, int slot, bool write, uint32_t count) {
  HbaCmdHeader *header = &info->cmd_list[slot];

  memset(header, 0, sizeof(HbaCmdHeader));

  header->cfl = sizeof(FisRegH2D) / sizeof(uint32_t);

  header->w = write ? 1 : 0;

  header->prdtl = 1;

  HbaCmdTable *table = info->cmd_table;

  memset(table, 0, sizeof(HbaCmdTable));

  table->prdt[0].dba = (uint32_t)(uint64_t)info->buffer;

  table->prdt[0].dbau = (uint32_t)((uint64_t)info->buffer >> 32);

  table->prdt[0].dbc = (count * 512) - 1;

  table->prdt[0].i = 1;

  return true;
}

static void create_fis(HbaCmdTable *table, uint8_t command, uint64_t lba, uint32_t count) {

  FisRegH2D *fis = (FisRegH2D *)table->cfis;

  memset(fis, 0, sizeof(FisRegH2D));

  fis->type = 0x27;

  fis->c = 1;

  fis->command = command;

  fis->device = 1 << 6;

  fis->lba0 = lba;
  fis->lba1 = lba >> 8;
  fis->lba2 = lba >> 16;

  fis->lba3 = lba >> 24;
  fis->lba4 = lba >> 32;
  fis->lba5 = lba >> 40;

  fis->count_low = count & 0xff;

  fis->count_high = count >> 8;
}

static bool submit_command(AhciPortInfo *info, uint8_t command, uint64_t lba, uint32_t count,
                           bool write) {
  HbaPort *port = info->port;

  port->is = (uint32_t)-1;

  int slot = find_slot(port);
  if (slot < 0)
    return false;

  setup_command(info, slot, write, count);
  create_fis(info->cmd_table, command, lba, count);

  port->ci = 1 << slot;

  return wait(port, slot);
}

bool read(uint8_t port_num, uint64_t lba, uint32_t count, void *buffer) {
  if (port_num >= 32 || !ports[port_num].active || !buffer || count == 0)
    return false;

  AhciPortInfo *info = &ports[port_num];
  uint8_t *dst = (uint8_t *)buffer;
  uint32_t remaining = count;

  while (remaining > 0) {
    uint32_t chunk = remaining > 128 ? 128 : remaining;

    if (!submit_command(info, ATA_CMD_READ_DMA_EXT, lba, chunk, false))
      return false;

    memcpy(dst, info->buffer, chunk * 512);
    dst += chunk * 512;
    lba += chunk;
    remaining -= chunk;
  }

  return true;
}

bool write(uint8_t port_num, uint64_t lba, uint32_t count, const void *buffer) {
  if (port_num >= 32 || !ports[port_num].active || !buffer || count == 0)
    return false;

  AhciPortInfo *info = &ports[port_num];
  const uint8_t *src = (const uint8_t *)buffer;
  uint32_t remaining = count;

  while (remaining > 0) {
    uint32_t chunk = remaining > 128 ? 128 : remaining;

    memcpy(info->buffer, src, chunk * 512);

    if (!submit_command(info, ATA_CMD_WRITE_DMA_EXT, lba, chunk, true))
      return false;

    src += chunk * 512;
    lba += chunk;
    remaining -= chunk;
  }

  return true;
}

static bool identify(AhciPortInfo *info) {

  HbaPort *port = info->port;

  int slot = find_slot(port);

  if (slot < 0)
    return false;

  setup_command(info, slot, false, 1);

  create_fis(info->cmd_table, ATA_CMD_IDENTIFY, 0, 0);

  port->ci = 1 << slot;

  if (!wait(port, slot)) {
    logger::warn("[AHCI] port %d: IDENTIFY failed (tfd=%x serr=%x is=%x)", info->number, port->tfd,
                 port->serr, port->is);
    return false;
  }

  // wait() only confirms the slot's CI bit cleared without a task-file
  // error -- it says nothing about whether the HBA actually moved any
  // data into info->buffer. PRDBC (bytes transferred, written back into
  // the command header by the HBA on completion) is the real signal;
  // a command that "completes" clean but transferred 0 bytes is
  // indistinguishable from a real success without checking it, and
  // reads back as an all-zero IDENTIFY buffer -- exactly the symptom
  // being chased here.
  uint32_t transferred = info->cmd_list[slot].prdbc;

  uint16_t *id = (uint16_t *)info->buffer;

  logger::info(
      "[AHCI] port %d: IDENTIFY prdbc=%u id[0]=%x id[60..61]=%x,%x id[100..103]=%x,%x,%x,%x",
      info->number, transferred, id[0], id[60], id[61], id[100], id[101], id[102], id[103]);

  if (transferred == 0) {
    logger::warn("[AHCI] port %d: IDENTIFY reported success but transferred 0 bytes", info->number);
    return false;
  }

  uint64_t sectors = ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) |
                     ((uint64_t)id[101] << 16) | ((uint64_t)id[100]);

  if (!sectors) {
    sectors = ((uint32_t)id[61] << 16) | id[60];
  }

  info->sectors = sectors;

  ident_swap(info->model, &id[27], 20);
  info->model[40] = 0;
  for (int i = 39; i >= 0; i--) {
    if (info->model[i] == ' ')
      info->model[i] = 0;
    else
      break;
  }

  ident_swap(info->serial, &id[10], 10);
  info->serial[20] = 0;
  for (int i = 19; i >= 0; i--) {
    if (info->serial[i] == ' ')
      info->serial[i] = 0;
    else
      break;
  }

  return true;
}

static bool port_link_up(HbaPort *p) {
  uint32_t ssts = p->ssts;
  uint8_t det = ssts & 0xf;
  uint8_t ipm = (ssts >> 8) & 0xf;
  return det == 3 && ipm == 1;
}

// Forces a COMRESET on the port's PHY (AHCI spec 10.4.2). Not every
// BIOS/hypervisor leaves ports already link-trained the way QEMU's
// SeaBIOS does -- without this, a port can read DET==0 ("no device
// detected yet") for a drive that's genuinely attached, and probe_port()
// would silently skip it.
static void reset_port(HbaPort *p) {
  p->sctl = (p->sctl & ~0xfu) | 1u; // DET = 1: initiate COMRESET

  // Spec requires holding COMRESET for at least 1ms. There's no timer
  // available this early in boot (ports are probed before the
  // scheduler/PIT-driven timer is usable here), so this is a coarse
  // busy-wait substitute -- fine for a one-time boot step. 200k pause
  // iterations is comfortably above 1ms on any modern CPU and ~5x faster
  // than the previous 1M-loop, which dominated port-probe time on fast
  // machines where most ports were already trained.
  for (volatile int i = 0; i < 200000; i++)
    asm volatile("pause");

  p->sctl &= ~0xfu; // DET = 0: release COMRESET, let the link train

  // Once COMRESET is released, devices on QEMU/typical PC hardware come
  // up in well under 1ms. 500k iterations is ~5ms -- if the link hasn't
  // trained by then the port is genuinely empty and we shouldn't keep
  // spinning.
  uint32_t timeout = 500000;
  while (timeout-- && !port_link_up(p))
    asm volatile("pause");
}

constexpr uint32_t ATA_DEV_BUSY = 0x80;
constexpr uint32_t ATA_DEV_DRQ = 0x08;

// The link reporting "up" (DET==3) only means the PHY has trained --
// the device behind it can still be asserting BSY/DRQ while it finishes
// its own internal power-on/reset sequence. Reading PxSIG or issuing a
// command (IDENTIFY) during that window is a known way to get a
// signature/data that looks valid-ish but is actually stale or blank --
// which is exactly what an IDENTIFY that "succeeds" but returns an
// all-zero sector count looks like.
static bool wait_not_busy(HbaPort *p) {
  uint32_t timeout = 1000000;
  while (timeout--) {
    if (!(p->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)))
      return true;
    asm volatile("pause");
  }
  return false;
}

namespace atapi {
static bool atapi_identify(AhciPortInfo *info);
}

static void probe_port(HbaMem *hba, uint8_t num) {

  HbaPort *port = &hba->ports[num];

  if (!port_link_up(port)) {
    reset_port(port);

    if (!port_link_up(port))
      return;
  }

  // Reset can leave stale bits set (e.g. PxSERR.DIAG.X from the
  // COMRESET itself) -- clear them before this port does anything else,
  // same as stop_cmd()/start_cmd() below expect a clean slate.
  port->serr = port->serr;

  if (!wait_not_busy(port))
    return;

  bool is_atapi = (port->sig == SATA_SIG_ATAPI);

  if (port->sig != SATA_SIG_ATA && !is_atapi)
    return;

  stop_cmd(port);

  HbaCmdHeader *cmd = (HbaCmdHeader *)heap::kmalloc(1024);

  void *fis = heap::kmalloc(256);

  HbaCmdTable *table = (HbaCmdTable *)heap::kmalloc(sizeof(HbaCmdTable));

  void *buffer = heap::kmalloc(65536);

  memset(cmd, 0, 1024);
  memset(fis, 0, 256);
  memset(table, 0, sizeof(HbaCmdTable));

  port->clb = (uint32_t)(uint64_t)cmd;

  port->clbu = (uint32_t)((uint64_t)cmd >> 32);

  port->fb = (uint32_t)(uint64_t)fis;

  port->fbu = (uint32_t)((uint64_t)fis >> 32);

  cmd[0].ctba = (uint32_t)(uint64_t)table;

  cmd[0].ctbau = (uint32_t)((uint64_t)table >> 32);

  ports[num].number = num;
  ports[num].port = port;
  ports[num].cmd_list = cmd;
  ports[num].cmd_table = table;
  ports[num].fis = fis;
  ports[num].buffer = buffer;
  ports[num].active = true;
  ports[num].is_atapi = is_atapi;

  start_cmd(port);

  if (is_atapi) {
    if (atapi::atapi_identify(&ports[num])) {
      logger::info("[AHCI] ATAPI port %d: %u sectors (CD/DVD)", num, (uint32_t)ports[num].sectors);
    } else {
      ports[num].sectors = 0xFFFFFFFF;
      logger::info("[AHCI] ATAPI port %d: CD/DVD (capacity unknown, using max)", num);
    }
  } else {
    if (identify(&ports[num])) {
      logger::info("[AHCI] SATA port %d: %llu sectors %s [%s]", num, ports[num].sectors,
                   ports[num].model, ports[num].serial);
    }
  }
}

struct MbrPartition {

  uint8_t boot;

  uint8_t start_head;
  uint8_t start_sector;
  uint8_t start_cylinder;

  uint8_t type;

  uint8_t end_head;
  uint8_t end_sector;
  uint8_t end_cylinder;

  uint32_t lba_start;
  uint32_t sectors;

} __attribute__((packed));

struct Mbr {

  uint8_t boot_code[446];

  MbrPartition partitions[4];

  uint16_t signature;

} __attribute__((packed));

struct DevContext {

  uint8_t port;

  uint64_t offset;

  uint64_t sectors;

  bool is_cdrom;
};

static DevContext dev_ctx[64];

static int dev_count = 0;

static fs::vfs::DevOps dev_ops[64];

static size_t device_read(int id, char *buffer, size_t size, size_t offset) {

  DevContext *ctx = &dev_ctx[id];

  if (ctx->is_cdrom) {
    uint32_t block_size = 2048;
    uint64_t lba = offset / block_size;
    uint32_t sector_offset = offset % block_size;

    static uint8_t temp[4096];

    if (!atapi::read(ctx->port, lba, 1, temp))
      return 0;

    size_t available = block_size - sector_offset;
    if (size > available)
      size = available;

    memcpy(buffer, temp + sector_offset, size);
    return size;
  }

  uint64_t lba = ctx->offset + (offset / 512);

  uint32_t sector_offset = offset % 512;

  if (size == 512 && sector_offset == 0) {

    if (read(ctx->port, lba, 1, buffer)) {
      return 512;
    }

    return 0;
  }

  static uint8_t temp[512];

  if (!read(ctx->port, lba, 1, temp)) {
    return 0;
  }

  size_t available = 512 - sector_offset;

  if (size > available)
    size = available;

  memcpy(buffer, temp + sector_offset, size);

  return size;
}

static size_t device_write(int id, const char *buffer, size_t size, size_t offset) {

  DevContext *ctx = &dev_ctx[id];

  if (ctx->is_cdrom)
    return 0;

  uint64_t lba = ctx->offset + (offset / 512);
  uint32_t sector_offset = offset % 512;

  if (size == 512 && sector_offset == 0) {
    if (write(ctx->port, lba, 1, buffer))
      return 512;
    return 0;
  }

  static uint8_t temp[512];

  if (!read(ctx->port, lba, 1, temp))
    return 0;

  size_t available = 512 - sector_offset;
  if (size > available)
    size = available;

  memcpy(temp + sector_offset, buffer, size);

  if (!write(ctx->port, lba, 1, temp))
    return 0;

  return size;
}

// Linux-standard block ioctl numbers -- kept local since these are
// AHCI-block-device-specific and there's no sys/blkio.h yet in the libc.
constexpr unsigned long BLKGETSIZE64 = 0x80081272;
constexpr unsigned long BLKGETSIZE = 0x1260;
constexpr unsigned long BLKSSZGET = 0x1268;

static int device_ioctl(int id, unsigned long req, void *arg) {
  if (!arg)
    return -1;

  DevContext *ctx = &dev_ctx[id];

  switch (req) {
  case BLKGETSIZE64:
    *(uint64_t *)arg = ctx->is_cdrom ? (ctx->sectors * 2048ULL) : (ctx->sectors * 512ULL);
    return 0;

  case BLKGETSIZE:
    *(unsigned long *)arg = (unsigned long)ctx->sectors;
    return 0;

  case BLKSSZGET:
    *(int *)arg = ctx->is_cdrom ? 2048 : 512;
    return 0;
  }

  return -1;
}

#define MAKE_DEVICE(ID)                                                                            \
  dev_ops[ID].read = [](char *b, size_t s, size_t o) { return device_read(ID, b, s, o); };         \
  dev_ops[ID].write = [](const char *b, size_t s, size_t o) { return device_write(ID, b, s, o); }; \
  dev_ops[ID].ioctl = [](unsigned long req, void *arg) { return device_ioctl(ID, req, arg); };

static void register_disk_device(const char *name, uint8_t port, uint64_t offset,
                                 uint64_t sectors) {

  if (dev_count >= 64)
    return;

  int id = dev_count++;

  dev_ctx[id].port = port;

  dev_ctx[id].offset = offset;

  dev_ctx[id].sectors = sectors;

  dev_ctx[id].is_cdrom = false;

  switch (id) {

  case 0:
    MAKE_DEVICE(0);
    break;

  case 1:
    MAKE_DEVICE(1);
    break;

  case 2:
    MAKE_DEVICE(2);
    break;

  case 3:
    MAKE_DEVICE(3);
    break;

  case 4:
    MAKE_DEVICE(4);
    break;

  case 5:
    MAKE_DEVICE(5);
    break;

  default:
    return;
  }

  fs::devfs::register_device(name, &dev_ops[id]);

  logger::info("[AHCI] Registered /dev/%s", name);
}

namespace atapi {

static bool send_packet(uint8_t port_num, const uint8_t *packet, size_t packet_len, void *buffer,
                        size_t buffer_len, bool write) {
  if (port_num >= 32 || !ports[port_num].active || !ports[port_num].is_atapi)
    return false;

  AhciPortInfo *info = &ports[port_num];
  HbaPort *port = info->port;

  port->is = (uint32_t)-1;

  int slot = find_slot(port);
  if (slot < 0)
    return false;

  HbaCmdHeader *header = &info->cmd_list[slot];
  memset(header, 0, sizeof(HbaCmdHeader));
  header->cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
  header->w = write ? 1 : 0;
  header->a = 1;
  header->prdtl = (buffer_len > 0) ? 1 : 0;

  HbaCmdTable *table = info->cmd_table;
  memset(table, 0, sizeof(HbaCmdTable));

  memcpy(table->acmd, packet, packet_len < 16 ? packet_len : 16);

  FisRegH2D *fis = (FisRegH2D *)table->cfis;
  memset(fis, 0, sizeof(FisRegH2D));
  fis->type = 0x27;
  fis->c = 1;
  fis->command = ATA_CMD_PACKET;
  fis->device = 0;

  if (buffer_len > 0) {
    table->prdt[0].dba = (uint32_t)(uint64_t)info->buffer;
    table->prdt[0].dbau = (uint32_t)((uint64_t)info->buffer >> 32);
    table->prdt[0].dbc = buffer_len - 1;
    table->prdt[0].i = 1;

    if (write)
      memcpy(info->buffer, buffer, buffer_len);
  }

  port->ci = 1 << slot;

  if (!wait(port, slot))
    return false;

  if (!write && buffer_len > 0)
    memcpy(buffer, info->buffer, buffer_len);

  return true;
}

bool is_atapi_device(uint8_t port_num) {
  if (port_num >= 32)
    return false;
  return ports[port_num].active && ports[port_num].is_atapi;
}

static bool atapi_identify(AhciPortInfo *info) {
  HbaPort *port = info->port;

  int slot = find_slot(port);
  if (slot < 0)
    return false;

  HbaCmdHeader *header = &info->cmd_list[slot];
  memset(header, 0, sizeof(HbaCmdHeader));
  header->cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
  header->w = 0;
  header->prdtl = 1;

  HbaCmdTable *table = info->cmd_table;
  memset(table, 0, sizeof(HbaCmdTable));

  table->prdt[0].dba = (uint32_t)(uint64_t)info->buffer;
  table->prdt[0].dbau = (uint32_t)((uint64_t)info->buffer >> 32);
  table->prdt[0].dbc = 511;
  table->prdt[0].i = 1;

  FisRegH2D *fis = (FisRegH2D *)table->cfis;
  memset(fis, 0, sizeof(FisRegH2D));
  fis->type = 0x27;
  fis->c = 1;
  fis->command = ATA_CMD_IDENTIFY_PACKET;
  fis->device = 0;

  port->ci = 1 << slot;

  if (!wait(port, slot))
    return false;

  uint16_t *id = (uint16_t *)info->buffer;

  uint32_t cap_low = id[60] | ((uint32_t)id[61] << 16);
  uint64_t cap_high =
      ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) | ((uint64_t)id[101] << 16) | id[100];

  info->sectors = cap_high ? cap_high : cap_low;

  return true;
}

static bool atapi_read_capacity(uint8_t port_num, uint32_t *lba, uint32_t *block_size) {
  uint8_t packet[12];
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x25;

  uint32_t response[2];
  if (!send_packet(port_num, packet, 12, response, 8, false))
    return false;

  *lba = ((uint32_t)response[0] << 24) | ((response[0] >> 8) & 0xFF0000) |
         ((response[0] >> 16) & 0xFF00) | (response[0] >> 24);

  *block_size =
      ((uint32_t)response[1] << 24) | ((response[1] >> 8) & 0xFF0000) |
      ((response[1] >> 16) & 0xFF00) | (response[1] >> 24);

  return true;
}

bool read(uint8_t port_num, uint32_t lba, uint32_t count, void *buf) {
  if (port_num >= 32 || !ports[port_num].active || !ports[port_num].is_atapi)
    return false;

  if (!buf || count == 0)
    return false;

  uint8_t packet[12];
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x28;
  packet[2] = (lba >> 24) & 0xFF;
  packet[3] = (lba >> 16) & 0xFF;
  packet[4] = (lba >> 8) & 0xFF;
  packet[5] = lba & 0xFF;
  packet[7] = (count >> 8) & 0xFF;
  packet[8] = count & 0xFF;

  return send_packet(port_num, packet, 12, buf, count * 2048, false);
}

uint32_t get_capacity_sectors(uint8_t port_num) {
  if (port_num >= 32 || !ports[port_num].active || !ports[port_num].is_atapi)
    return 0;

  uint32_t max_lba, block_size;
  if (atapi_read_capacity(port_num, &max_lba, &block_size)) {
    ports[port_num].sectors = max_lba + 1;
    return max_lba + 1;
  }

  if (ports[port_num].sectors > 0 && ports[port_num].sectors != 0xFFFFFFFF)
    return ports[port_num].sectors;

  return 0;
}

static void register_device(const char *name, uint8_t port_num) {
  if (dev_count >= 64)
    return;

  int id = dev_count++;
  dev_ctx[id].port = port_num;
  dev_ctx[id].offset = 0;
  dev_ctx[id].is_cdrom = true;

  uint32_t sectors = get_capacity_sectors(port_num);
  dev_ctx[id].sectors = sectors;

  switch (id) {
  case 0: MAKE_DEVICE(0); break;
  case 1: MAKE_DEVICE(1); break;
  case 2: MAKE_DEVICE(2); break;
  case 3: MAKE_DEVICE(3); break;
  case 4: MAKE_DEVICE(4); break;
  case 5: MAKE_DEVICE(5); break;
  default: return;
  }

  fs::devfs::register_device(name, &dev_ops[id]);
  logger::info("[ATAPI] Registered /dev/%s (%u sectors)", name, sectors);
}

} // namespace atapi

static bool init_controller(pci::PciDevice *dev) {
  uint64_t abar_phys = dev->bar[5] & ~0xF;
  if (!abar_phys)
    return false;

  if (controller_count >= 4) {
    logger::warn("[AHCI] too many controllers, skipping");
    return false;
  }

  paging::map_page(paging::kernel_pml4, abar_phys, abar_phys, PAGE_PRESENT | PAGE_WRITABLE);

  HbaMem *hba = (HbaMem *)abar_phys;
  controllers[controller_count].abar = hba;
  controllers[controller_count].bus = dev->bus;
  controllers[controller_count].slot = dev->slot;
  controllers[controller_count].func = dev->func;

  uint32_t cap = hba->cap;
  int num_ports = (cap & CAP_NP_MASK) + 1;
  int num_slots = ((cap & CAP_NCS) >> CAP_NCS_SHIFT) + 1;

  logger::info("[AHCI] Controller %02x:%02x.%d ABAR=%llx ports=%d slots=%d", dev->bus, dev->slot,
               dev->func, abar_phys, num_ports, num_slots);

  hba->ghc |= 1 << 31;

  uint32_t pi = hba->pi;

  for (int i = 0; i < 32; i++) {
    if (pi & (1 << i))
      probe_port(hba, i);
  }

  controller_count++;
  return true;
}

void init() {
  pci::PciDevice device;

  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        uint32_t id = pci::read_config32((uint8_t)bus, slot, func, 0x00);
        if ((id & 0xFFFF) == 0xFFFF)
          continue;

        uint32_t class_dword = pci::read_config32((uint8_t)bus, slot, func, 0x08);
        uint8_t pi = (uint8_t)(class_dword >> 8);
        uint8_t sc = (uint8_t)(class_dword >> 16);
        uint8_t cc = (uint8_t)(class_dword >> 24);

        if (cc == 0x01 && sc == 0x06 && pi == 0x01) {
          pci::PciDevice dev;
          dev.bus = (uint8_t)bus;
          dev.slot = slot;
          dev.func = func;
          dev.vendor_id = id & 0xFFFF;
          dev.device_id = (id >> 16) & 0xFFFF;
          dev.class_code = cc;
          dev.subclass = sc;
          dev.prog_if = pi;
          dev.header_type = pci::read_config8((uint8_t)bus, slot, func, 0x0E);

          for (int i = 0; i < 6; i++)
            dev.bar[i] = pci::read_config32((uint8_t)bus, slot, func, 0x10 + i * 4);

          pci::enable_bus_mastering(&dev);
          init_controller(&dev);
        }

        if (func == 0) {
          uint8_t header = pci::read_config8((uint8_t)bus, slot, func, 0x0E);
          if (!(header & 0x80))
            break;
        }
      }
    }
  }

  if (controller_count == 0) {
    logger::warn("[AHCI] no controllers found");
    return;
  }

  int disk = 0;
  for (int i = 0; i < 32; i++) {
    if (!ports[i].active || ports[i].is_atapi)
      continue;

    char name[16];
    ksnprintf(name, sizeof(name), "sd%c", 'a' + disk);
    register_disk_device(name, i, 0, ports[i].sectors);

    Mbr mbr;
    if (read(i, 0, 1, &mbr) && mbr.signature == 0xAA55) {
      for (int p = 0; p < 4; p++) {
        if (mbr.partitions[p].type == 0 || mbr.partitions[p].sectors == 0)
          continue;
        char part[32];
        ksnprintf(part, sizeof(part), "%s%d", name, p + 1);
        register_disk_device(part, i, mbr.partitions[p].lba_start, mbr.partitions[p].sectors);
      }
    }

    disk++;
  }

  int cdrom = 0;
  for (int i = 0; i < 32; i++) {
    if (!ports[i].active || !ports[i].is_atapi)
      continue;

    char name[16];
    ksnprintf(name, sizeof(name), "sr%d", cdrom);
    atapi::register_device(name, i);
    cdrom++;
  }

  initialized = true;
  logger::info("[AHCI] initialized %d controller(s)", controller_count);
}

} // namespace drivers::ahci
