#ifndef NSDP_PACKET_H
#define NSDP_PACKET_H

#include "nsdp_types.h"
#include "nsdp_property.h"

#define NSDP_PKT_HEADER_SIZE		0x20
#define NSDP_PKT_TRAILER_SIZE		0x04
#define NSDP_PROPERTY_HEADER_SIZE	0x04

#define NSDP_OP_READ_REQUEST		0x01
#define NSDP_OP_READ_RESPONSE		0x02
#define NSDP_OP_WRITE_REQUEST		0x03
#define NSDP_OP_WRITE_RESPONSE		0x04

#define NSDP_OP_IS_REQUEST(op)	\
  ((op) == NSDP_OP_READ_REQUEST || (op) == NSDP_OP_WRITE_REQUEST)
#define NSDP_OP_IS_RESPONSE(op)	\
  ((op) == NSDP_OP_READ_RESPONSE || (op) == NSDP_OP_WRITE_RESPONSE)

#define NSDP_OP_IS_READ(op)	\
  ((op) == NSDP_OP_READ_REQUEST || (op) == NSDP_OP_READ_RESPONSE)
#define NSDP_OP_IS_WRITE(op)	\
  ((op) == NSDP_OP_WRITE_REQUEST || (op) == NSDP_OP_WRITE_RESPONSE)


typedef struct nsdp_packet {
  nsdp_op_t		op;
  nsdp_mac_t		client_mac;
  nsdp_mac_t		server_mac;
  nsdp_seq_no_t		seq_no;
  struct list_head	properties;
} nsdp_packet_t;

void nsdp_packet_init(nsdp_packet_t *pkt);

nsdp_packet_t *nsdp_packet_new(void);

void nsdp_packet_uninit(nsdp_packet_t *pkt);

void nsdp_packet_free(nsdp_packet_t *pkt);

int nsdp_packet_length(const nsdp_packet_t *pkt);

int nsdp_packet_add_property(nsdp_packet_t *pkt, nsdp_property_t *property);

int nsdp_packet_add_properties_terminator(nsdp_packet_t *pkt);

int nsdp_packet_write(const nsdp_packet_t *pkt, void *buffer, unsigned max_size);

int nsdp_packet_read(nsdp_packet_t *pkt, const void *buffer, unsigned size);

#define nsdp_packet_for_each_property(pkt, t) \
  list_for_each_entry((t), &(pkt)->properties, list)

#endif /* NSDP_PACKET_H */
