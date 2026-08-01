#pragma once

#include <stdint.h>
#include <stddef.h>

namespace drivers::ahci {

void init();

bool read(uint8_t port_num, uint64_t lba, uint32_t count, void *buf);
bool write(uint8_t port_num, uint64_t lba, uint32_t count, const void *buf);
uint64_t get_capacity_sectors(uint8_t port_num);

bool is_available();

} // namespace drivers::ahci
