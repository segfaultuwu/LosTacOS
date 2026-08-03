#pragma once

#include <stdint.h>
#include <stddef.h>

namespace drivers::ahci {

void init();

bool read(uint8_t port_num, uint64_t lba, uint32_t count, void *buf);
bool write(uint8_t port_num, uint64_t lba, uint32_t count, const void *buf);
uint64_t get_capacity_sectors(uint8_t port_num);

bool is_available();

namespace atapi {

bool is_atapi_device(uint8_t port_num);
bool read(uint8_t port_num, uint32_t lba, uint32_t count, void *buf);
uint32_t get_capacity_sectors(uint8_t port_num);

} // namespace atapi

} // namespace drivers::ahci
