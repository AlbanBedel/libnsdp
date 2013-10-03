#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "nsdp_packet.h"

static inline void nsdp_set_u16be(void* buf, uint16_t val)
{
  ((uint8_t*)buf)[1] = (val >> 0) & 0xFF;
  ((uint8_t*)buf)[0] = (val >> 8) & 0xFF;
}

static inline uint16_t nsdp_get_u16be(const void *buf)
{
  return (((uint16_t)(((uint8_t*)buf)[1])) << 0) |
    (((uint16_t)(((uint8_t*)buf)[0])) << 8);
}

void nsdp_packet_init(nsdp_packet_t *pkt)
{
  memset(pkt, 0, sizeof(pkt));
  INIT_LIST_HEAD(&pkt->properties);
}

nsdp_packet_t *nsdp_packet_new(void)
{
  nsdp_packet_t *pkt = calloc(1, sizeof(*pkt));

  if (pkt)
    nsdp_packet_init(pkt);

  return pkt;
}

void nsdp_packet_uninit(nsdp_packet_t *pkt)
{
  if (!pkt)
    return;

  while(!list_empty(&pkt->properties))
    nsdp_property_free(list_first_entry(&pkt->properties,
                                        nsdp_property_t, list));
}

void nsdp_packet_free(nsdp_packet_t *pkt)
{
  nsdp_packet_uninit(pkt);
  free(pkt);
}

int nsdp_packet_length(const nsdp_packet_t *pkt)
{
  int length = NSDP_PKT_HEADER_SIZE;
  nsdp_property_t *prop;

  if (!pkt)
    return -EINVAL;

  list_for_each_entry(prop, &pkt->properties, list)
    length += NSDP_PROPERTY_HEADER_SIZE + prop->length;

  return length;
}

int nsdp_packet_has_terminator(nsdp_packet_t *pkt)
{
  nsdp_property_t *last;

  if (!pkt)
    return -EINVAL;

  if (list_empty(&pkt->properties))
    return 0;

  last = list_entry(pkt->properties.prev, nsdp_property_t, list);
  return (last->tag == NSDP_PROPERTY_TERMINATOR);
}

int nsdp_packet_add_property(nsdp_packet_t *pkt, nsdp_property_t *property)
{
  int term;

  if (!property)
    return -EINVAL;

  term = nsdp_packet_has_terminator(pkt);
  if (term)
    return term < 0 ? term : -EBADMSG;

  list_add_tail(&property->list, &pkt->properties);
  return 0;
}

int nsdp_packet_add_properties_terminator(nsdp_packet_t *pkt)
{
  nsdp_property_t *term;
  int err;

  if (!pkt)
    return -EINVAL;

  term = nsdp_property_new(NSDP_PROPERTY_TERMINATOR, 0);
  if (!term)
    return -ENOMEM;

  err = nsdp_packet_add_property(pkt, term);
  if (err)
    nsdp_property_free(term);

  return err;
}

int nsdp_packet_write(const nsdp_packet_t *pkt, void *buffer,
                      unsigned max_size)
{
  int length = nsdp_packet_length(pkt);
  uint8_t *data = buffer;
  unsigned pos = NSDP_PKT_HEADER_SIZE;
  nsdp_property_t *prop;

  if (!pkt || !buffer)
    return -EINVAL;
  if (length < 0)
    return length;
  if (length > max_size)
    return -E2BIG;

  data[0x00] = 1; // version
  data[0x01] = pkt->op;
  memcpy(data+0x08, pkt->client_mac, sizeof(nsdp_mac_t));
  memcpy(data+0x0e, pkt->server_mac, sizeof(nsdp_mac_t));
  nsdp_set_u16be(data+0x16, pkt->seq_no);
  memcpy(data+0x18, "NSDP", 4); // signature

  list_for_each_entry(prop, &pkt->properties, list) {
    nsdp_set_u16be(data+pos, prop->tag);
    nsdp_set_u16be(data+pos+2, prop->length);
    if (prop->length > 0)
      memcpy(data+pos+NSDP_PROPERTY_HEADER_SIZE, prop->data, prop->length);
    pos += NSDP_PROPERTY_HEADER_SIZE + prop->length;
  }

  assert(pos == length);
  return pos;
}

int nsdp_packet_read(nsdp_packet_t *pkt, const void *buffer, unsigned size)
{
  const uint8_t* data = buffer;
  unsigned pos = NSDP_PKT_HEADER_SIZE;
  int err;

  if (!pkt || !buffer ||
      size < NSDP_PKT_HEADER_SIZE + NSDP_PROPERTY_HEADER_SIZE)
    return -EINVAL;

  if (data[0] != 1 || memcmp(data+0x18, "NSDP", 4))
    return -EINVAL;

  pkt->op = data[1];
  memcpy(pkt->client_mac, data+0x08, sizeof(nsdp_mac_t));
  memcpy(pkt->server_mac, data+0x0e, sizeof(nsdp_mac_t));
  pkt->seq_no = nsdp_get_u16be(data+0x16);

  while (pos + NSDP_PROPERTY_HEADER_SIZE <= size) {
    nsdp_property_t *prop =
      nsdp_property_from_data(nsdp_get_u16be(data+pos),
                              nsdp_get_u16be(data+pos+2),
                              data+pos+NSDP_PROPERTY_HEADER_SIZE);
    if (!prop)
      return -ENOMEM;
    err = nsdp_packet_add_property(pkt, prop);
    if (err) {
      nsdp_property_free(prop);
      return err;
    }
    pos += NSDP_PROPERTY_HEADER_SIZE + prop->length;
  }

  return pos;
}
