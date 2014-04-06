#ifndef NSDP_TYPES_H
#define NSDP_TYPES_H

#include <inttypes.h>
#include "list.h"

typedef uint16_t 	nsdp_tag_t;
typedef uint16_t 	nsdp_length_t;

typedef uint8_t		nsdp_version_t;
typedef uint8_t		nsdp_op_t;
typedef uint8_t 	nsdp_mac_t[6];
typedef uint16_t	nsdp_seq_no_t;
typedef uint8_t 	nsdp_sig_t[4];

static inline void nsdp_set_u16be(void* buf, uint16_t val)
{
  ((uint8_t*)buf)[1] = (val >> 0) & 0xFF;
  ((uint8_t*)buf)[0] = (val >> 8) & 0xFF;
}

static inline uint16_t nsdp_get_u16be(const void *buf)
{
  return ((((uint16_t)(((uint8_t*)buf)[1])) << 0) |
          (((uint16_t)(((uint8_t*)buf)[0])) << 8));
}

static inline uint32_t nsdp_get_u32be(const void *buf)
{
  return (((uint32_t)nsdp_get_u16be((uint8_t*)buf + 2) << 0) |
          ((uint32_t)nsdp_get_u16be(buf) << 16));
}

static inline uint64_t nsdp_get_u64be(const void *buf)
{
  return (((uint64_t)nsdp_get_u32be((uint8_t*)buf + 4) << 0) |
          ((uint64_t)nsdp_get_u32be(buf) << 32));
}

#endif /* NSDP_TYPES_H */
