#pragma once

#include <stdint.h>

#define bswap_16(x) ((uint16_t)((((uint16_t)(x) & 0xff00) >> 8) | (((uint16_t)(x) & 0x00ff) << 8)))

#define bswap_32(x)                                                                                \
  ((uint32_t)((((uint32_t)(x) & 0xff000000) >> 24) | (((uint32_t)(x) & 0x00ff0000) >> 8) |         \
              (((uint32_t)(x) & 0x0000ff00) << 8) | (((uint32_t)(x) & 0x000000ff) << 24)))

#define bswap_64(x)                                                                                \
  ((uint64_t)((((uint64_t)(x) & 0xff00000000000000ULL) >> 56) |                                    \
              (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) |                                    \
              (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) |                                    \
              (((uint64_t)(x) & 0x000000ff00000000ULL) >> 8) |                                     \
              (((uint64_t)(x) & 0x00000000ff000000ULL) << 8) |                                     \
              (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) |                                    \
              (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) |                                    \
              (((uint64_t)(x) & 0x00000000000000ffULL) << 56)))
