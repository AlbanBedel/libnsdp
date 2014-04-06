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

#endif /* NSDP_TYPES_H */
