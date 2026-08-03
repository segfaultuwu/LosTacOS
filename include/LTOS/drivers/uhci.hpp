#pragma once

#include <stddef.h>
#include <stdint.h>

namespace drivers::uhci {

bool init();
bool can_transfer();

int control_in(uint8_t dev_addr, uint8_t ep, uint8_t bm_req_type, uint8_t req, uint16_t value,
               uint16_t index, void *buf, size_t len);

int control_out(uint8_t dev_addr, uint8_t ep, uint8_t bm_req_type, uint8_t req, uint16_t value,
                uint16_t index, const void *buf, size_t len);

int int_in(uint8_t dev_addr, uint8_t ep, bool data_toggle, bool is_ls, void *buf, size_t len);

} // namespace drivers::uhci
