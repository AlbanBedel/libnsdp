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

#endif /* NSDP_TYPES_H */
