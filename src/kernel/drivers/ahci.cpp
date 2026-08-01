#include "LTOS/drivers/ahci.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/pci.hpp"
#include "LTOS/fs/devfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include <string.h>

namespace drivers::ahci {

constexpr uint32_t SATA_SIG_ATA = 0x00000101;
constexpr uint32_t AHCI_DEV_NULL = 0;
constexpr uint32_t AHCI_DEV_SATA = 1;

constexpr uint8_t ATA_CMD_READ_DMA_EXT = 0x25;
constexpr uint8_t ATA_CMD_WRITE_DMA_EXT = 0x35;
constexpr uint8_t ATA_CMD_IDENTIFY = 0xEC;

constexpr uint32_t HBA_PxCMD_ST = 0x0001;
constexpr uint32_t HBA_PxCMD_FRE = 0x0010;
constexpr uint32_t HBA_PxCMD_FR = 0x4000;
constexpr uint32_t HBA_PxCMD_CR = 0x8000;

struct HbaPort {
  volatile uint32_t clb;
  volatile uint32_t clbu;
  volatile uint32_t fb;
  volatile uint32_t fbu;
  volatile uint32_t is;
  volatile uint32_t ie;
  volatile uint32_t cmd;
  volatile uint32_t rsv0;
  volatile uint32_t tfd;
  volatile uint32_t sig;
  volatile uint32_t ssts;
  volatile uint32_t sctl;
  volatile uint32_t serr;
  volatile uint32_t sact;
  volatile uint32_t ci;
  volatile uint32_t sntf;
  volatile uint32_t fbs;
  volatile uint32_t rsv1[11];
  volatile uint32_t vendor[4];
};

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
  uint8_t rsv[0xA0 - 0x2C];
  uint8_t vendor[0x100 - 0xA0];
  HbaPort ports[32];
};

struct HbaCmdHeader {
  uint8_t cfl : 5;
  uint8_t a : 1;
  uint8_t w : 1;
  uint8_t p : 1;

  uint8_t r : 1;
  uint8_t b : 1;
  uint8_t c : 1;
  uint8_t rsv0 : 1;
  uint8_t pmp : 4;

  uint16_t prdtl;
  volatile uint32_t prdbc;

  uint32_t ctba;
  uint32_t ctbau;

  uint32_t rsv1[4];
};

struct HbaPrdtEntry {
  uint32_t dba;
  uint32_t dbau;
  uint32_t rsv0;

  uint32_t dbc : 22;
  uint32_t rsv1 : 9;
  uint32_t i : 1;
};

struct HbaCmdTable {
  uint8_t cfis[64];
  uint8_t acmd[16];
  uint8_t rsv[48];
  HbaPrdtEntry prdt_entry[1];
};

struct FisRegH2D {
  uint8_t fis_type;
  uint8_t pmport : 4;
  uint8_t rsv0 : 3;
  uint8_t c : 1;

  uint8_t command;
  uint8_t featurelow;

  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;

  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t featurehigh;

  uint8_t countlow;
  uint8_t counthigh;
  uint8_t icc;
  uint8_t control;

  uint8_t rsv1[4];
};

struct AhciPortInfo {
  uint8_t port_num;
  HbaPort *port;
  uint64_t sectors;
  bool active;
  HbaCmdHeader *cmd_list;
  void *fis;
  HbaCmdTable *cmd_table;
  void *buffer_phys;
};

static HbaMem *abar = nullptr;
static AhciPortInfo ports_info[32] = {};
static bool initialized = false;

static void stop_cmd(HbaPort *port) {
  port->cmd &= ~HBA_PxCMD_ST;
  port->cmd &= ~HBA_PxCMD_FRE;

  while (true) {
    if (port->cmd & HBA_PxCMD_FR)
      continue;
    if (port->cmd & HBA_PxCMD_CR)
      continue;
    break;
  }
}

static void start_cmd(HbaPort *port) {
  while (port->cmd & HBA_PxCMD_CR)
    ;
  port->cmd |= HBA_PxCMD_FRE;
  port->cmd |= HBA_PxCMD_ST;
}

static int find_cmdslot(HbaPort *port) {
  uint32_t slots = (port->ci | port->sact);
  for (int i = 0; i < 32; i++) {
    if ((slots & (1 << i)) == 0)
      return i;
  }
  return -1;
}

bool is_available() {
  return initialized;
}

uint64_t get_capacity_sectors(uint8_t port_num) {
  if (port_num < 32 && ports_info[port_num].active)
    return ports_info[port_num].sectors;
  return 0;
}

bool read(uint8_t port_num, uint64_t lba, uint32_t count, void *buf) {
  if (port_num >= 32 || !ports_info[port_num].active || !buf || count == 0)
    return false;

  AhciPortInfo *info = &ports_info[port_num];
  HbaPort *port = info->port;

  port->is = (uint32_t)-1; // Clear interrupt flags

  int slot = find_cmdslot(port);
  if (slot == -1)
    return false;

  HbaCmdHeader *cmdheader = &info->cmd_list[slot];
  cmdheader->cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
  cmdheader->w = 0; // Read
  cmdheader->prdtl = 1;

  HbaCmdTable *cmdtable = info->cmd_table;
  memset(cmdtable, 0, sizeof(HbaCmdTable));

  cmdtable->prdt_entry[0].dba = (uint32_t)(uint64_t)info->buffer_phys;
  cmdtable->prdt_entry[0].dbau = (uint32_t)((uint64_t)info->buffer_phys >> 32);
  cmdtable->prdt_entry[0].dbc = (count * 512) - 1; // 512 bytes per sector
  cmdtable->prdt_entry[0].i = 1;

  FisRegH2D *cmdfis = (FisRegH2D *)(&cmdtable->cfis[0]);
  memset(cmdfis, 0, sizeof(FisRegH2D));

  cmdfis->fis_type = 0x27;
  cmdfis->c = 1;
  cmdfis->command = ATA_CMD_READ_DMA_EXT;

  cmdfis->lba0 = (uint8_t)lba;
  cmdfis->lba1 = (uint8_t)(lba >> 8);
  cmdfis->lba2 = (uint8_t)(lba >> 16);
  cmdfis->device = 1 << 6; // LBA mode

  cmdfis->lba3 = (uint8_t)(lba >> 24);
  cmdfis->lba4 = (uint8_t)(lba >> 32);
  cmdfis->lba5 = (uint8_t)(lba >> 40);

  cmdfis->countlow = (uint8_t)(count & 0xFF);
  cmdfis->counthigh = (uint8_t)((count >> 8) & 0xFF);

  uint32_t spin = 0;
  while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
    spin++;
  }
  if (spin == 1000000)
    return false;

  port->ci = (1 << slot);

  while (true) {
    if ((port->ci & (1 << slot)) == 0)
      break;
    if (port->is & (1 << 30)) // Task file error
      return false;
  }

  if (port->is & (1 << 30))
    return false;

  memcpy(buf, info->buffer_phys, count * 512);
  return true;
}

bool write(uint8_t port_num, uint64_t lba, uint32_t count, const void *buf) {
  if (port_num >= 32 || !ports_info[port_num].active || !buf || count == 0)
    return false;

  AhciPortInfo *info = &ports_info[port_num];
  HbaPort *port = info->port;

  memcpy(info->buffer_phys, buf, count * 512);

  port->is = (uint32_t)-1;

  int slot = find_cmdslot(port);
  if (slot == -1)
    return false;

  HbaCmdHeader *cmdheader = &info->cmd_list[slot];
  cmdheader->cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
  cmdheader->w = 1; // Write
  cmdheader->prdtl = 1;

  HbaCmdTable *cmdtable = info->cmd_table;
  memset(cmdtable, 0, sizeof(HbaCmdTable));

  cmdtable->prdt_entry[0].dba = (uint32_t)(uint64_t)info->buffer_phys;
  cmdtable->prdt_entry[0].dbau = (uint32_t)((uint64_t)info->buffer_phys >> 32);
  cmdtable->prdt_entry[0].dbc = (count * 512) - 1;
  cmdtable->prdt_entry[0].i = 1;

  FisRegH2D *cmdfis = (FisRegH2D *)(&cmdtable->cfis[0]);
  memset(cmdfis, 0, sizeof(FisRegH2D));

  cmdfis->fis_type = 0x27;
  cmdfis->c = 1;
  cmdfis->command = ATA_CMD_WRITE_DMA_EXT;

  cmdfis->lba0 = (uint8_t)lba;
  cmdfis->lba1 = (uint8_t)(lba >> 8);
  cmdfis->lba2 = (uint8_t)(lba >> 16);
  cmdfis->device = 1 << 6;

  cmdfis->lba3 = (uint8_t)(lba >> 24);
  cmdfis->lba4 = (uint8_t)(lba >> 32);
  cmdfis->lba5 = (uint8_t)(lba >> 40);

  cmdfis->countlow = (uint8_t)(count & 0xFF);
  cmdfis->counthigh = (uint8_t)((count >> 8) & 0xFF);

  uint32_t spin = 0;
  while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
    spin++;
  }
  if (spin == 1000000)
    return false;

  port->ci = (1 << slot);

  while (true) {
    if ((port->ci & (1 << slot)) == 0)
      break;
    if (port->is & (1 << 30))
      return false;
  }

  if (port->is & (1 << 30))
    return false;

  return true;
}

static bool identify(AhciPortInfo *info) {
  HbaPort *port = info->port;
  port->is = (uint32_t)-1;

  int slot = find_cmdslot(port);
  if (slot == -1)
    return false;

  HbaCmdHeader *cmdheader = &info->cmd_list[slot];
  cmdheader->cfl = sizeof(FisRegH2D) / sizeof(uint32_t);
  cmdheader->w = 0;
  cmdheader->prdtl = 1;

  HbaCmdTable *cmdtable = info->cmd_table;
  memset(cmdtable, 0, sizeof(HbaCmdTable));

  cmdtable->prdt_entry[0].dba = (uint32_t)(uint64_t)info->buffer_phys;
  cmdtable->prdt_entry[0].dbau = (uint32_t)((uint64_t)info->buffer_phys >> 32);
  cmdtable->prdt_entry[0].dbc = 511; // 512 bytes
  cmdtable->prdt_entry[0].i = 1;

  FisRegH2D *cmdfis = (FisRegH2D *)(&cmdtable->cfis[0]);
  memset(cmdfis, 0, sizeof(FisRegH2D));

  cmdfis->fis_type = 0x27;
  cmdfis->c = 1;
  cmdfis->command = ATA_CMD_IDENTIFY;

  port->ci = (1 << slot);

  uint32_t timeout = 1000000;
  while (timeout--) {
    if ((port->ci & (1 << slot)) == 0)
      break;
  }

  if (timeout == 0 || (port->is & (1 << 30)))
    return false;

  uint16_t *id_buf = (uint16_t *)info->buffer_phys;
  uint64_t lba48_sectors = ((uint64_t)id_buf[103] << 48) | ((uint64_t)id_buf[102] << 32) |
                           ((uint64_t)id_buf[101] << 16) | (uint64_t)id_buf[100];
  if (lba48_sectors == 0) {
    lba48_sectors = ((uint32_t)id_buf[61] << 16) | id_buf[60];
  }

  info->sectors = lba48_sectors;
  return true;
}

static void probe_port(HbaMem *hba, uint8_t pno) {
  uint32_t ssts = hba->ports[pno].ssts;
  uint8_t ipm = (ssts >> 8) & 0x0F;
  uint8_t det = ssts & 0x0F;

  if (det != 3 || ipm != 1)
    return;

  uint32_t sig = hba->ports[pno].sig;
  if (sig != SATA_SIG_ATA)
    return;

  HbaPort *port = &hba->ports[pno];
  stop_cmd(port);

  // Allocate 1KB command list, 256B FIS, 4KB command table, 64KB DMA buffer
  HbaCmdHeader *cmd_list = (HbaCmdHeader *)heap::kmalloc(1024 + 1024);
  cmd_list = (HbaCmdHeader *)(((uint64_t)cmd_list + 1023) & ~1023ULL);

  void *fis = heap::kmalloc(256 + 256);
  fis = (void *)(((uint64_t)fis + 255) & ~255ULL);

  HbaCmdTable *cmd_table = (HbaCmdTable *)heap::kmalloc(sizeof(HbaCmdTable) + 128);
  cmd_table = (HbaCmdTable *)(((uint64_t)cmd_table + 127) & ~127ULL);

  void *buf = heap::kmalloc(65536 + 4096);

  memset(cmd_list, 0, 1024);
  memset(fis, 0, 256);
  memset(cmd_table, 0, sizeof(HbaCmdTable));

  port->clb = (uint32_t)(uint64_t)cmd_list;
  port->clbu = (uint32_t)((uint64_t)cmd_list >> 32);

  port->fb = (uint32_t)(uint64_t)fis;
  port->fbu = (uint32_t)((uint64_t)fis >> 32);

  for (int i = 0; i < 32; i++) {
    cmd_list[i].prdtl = 8;
    cmd_list[i].ctba = (uint32_t)(uint64_t)cmd_table;
    cmd_list[i].ctbau = (uint32_t)((uint64_t)cmd_table >> 32);
  }

  AhciPortInfo *info = &ports_info[pno];
  info->port_num = pno;
  info->port = port;
  info->active = true;
  info->cmd_list = cmd_list;
  info->fis = fis;
  info->cmd_table = cmd_table;
  info->buffer_phys = buf;

  start_cmd(port);

  if (identify(info)) {
    uint32_t mb = (uint32_t)((info->sectors * 512) / (1024 * 1024));
    uint32_t sec = (uint32_t)info->sectors;
    logger::info("[AHCI] Port %d: SATA Hard Disk detected (%d MB, %d sectors)", pno, mb, sec);
  } else {
    logger::info("[AHCI] Port %d: SATA Hard Disk detected", pno);
  }
}

/*
 * DevFS Ops for /dev/sda
 */
static size_t sda_read(char *buf, size_t len, size_t offset) {
  uint64_t lba = offset / 512;
  uint32_t count = (len + 511) / 512;
  if (count == 0)
    count = 1;

  static char sector_buf[512];
  if (len == 512 && (offset % 512 == 0)) {
    if (read(0, lba, 1, buf))
      return 512;
    return 0;
  }

  if (read(0, lba, 1, sector_buf)) {
    size_t off_in_sec = offset % 512;
    size_t avail = 512 - off_in_sec;
    size_t n = len < avail ? len : avail;
    memcpy(buf, sector_buf + off_in_sec, n);
    return n;
  }
  return 0;
}

static size_t sda_write(const char *buf, size_t len, size_t offset) {
  uint64_t lba = offset / 512;
  if (len == 512 && (offset % 512 == 0)) {
    if (write(0, lba, 1, buf))
      return 512;
    return 0;
  }
  return 0;
}

static fs::vfs::DevOps sda_ops = {.write = sda_write, .read = sda_read, .ioctl = nullptr};

void init() {
  pci::PciDevice dev;
  if (!pci::find_device_by_class(0x01, 0x06, 0x01, &dev)) { // Class 1, Subclass 6, ProgIF 1
    // Try any SATA subclass
    if (!pci::find_device_by_class(0x01, 0x06, 0xFF, &dev)) {
      logger::warn("[AHCI] No AHCI controller found on PCI bus.\n");
      return;
    }
  }

  logger::info("[AHCI] Found SATA AHCI Controller at PCI %02x:%02x.%d (ABAR: 0x%x)\n", dev.bus,
               dev.slot, dev.func, dev.bar[5]);

  pci::enable_bus_mastering(&dev);

  uint64_t abar_phys = dev.bar[5] & ~0xF;
  if (abar_phys == 0) {
    logger::warn("[AHCI] ABAR address is null!\n");
    return;
  }

  // Identity map ABAR region (4KB)
  paging::map_page(paging::kernel_pml4, abar_phys, abar_phys, PAGE_PRESENT | PAGE_WRITABLE);

  abar = (HbaMem *)abar_phys;

  // Enable AHCI mode
  abar->ghc |= (1U << 31);

  uint32_t pi = abar->pi;
  for (int i = 0; i < 32; i++) {
    if (pi & (1 << i)) {
      probe_port(abar, (uint8_t)i);
    }
  }

  if (ports_info[0].active) {
    fs::devfs::register_device("sda", &sda_ops);
    logger::info("[AHCI] Registered /dev/sda block device.\n");
  }

  initialized = true;
}

} // namespace drivers::ahci
