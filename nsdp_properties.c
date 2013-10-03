#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "nsdp_property.h"

#define NSDP_PROPERTY_DESC(tg,n,d,tp)      \
  {                                        \
    .tag  = NSDP_PROPERTY_##tg,            \
    .name = n,                             \
    .desc = d,                             \
    .type = &nsdp_property_type_##tp       \
  }

static const struct nsdp_property_desc nsdp_property_desc[] = {
  NSDP_PROPERTY_DESC(MODEL, "model", "Hardware Model", str),
  NSDP_PROPERTY_DESC(HOSTNAME, "hostname", "Hostname", str),
  NSDP_PROPERTY_DESC(MAC, "mac", "Management Mac Adress", str),
  NSDP_PROPERTY_DESC(IP, "ip", "Management IP Address", ip4),
  NSDP_PROPERTY_DESC(NETMASK, "netmask", "Management IP Netmask", ip4),
  NSDP_PROPERTY_DESC(GATEWAY, "gateway", "Management IP Gateway", ip4),
  NSDP_PROPERTY_DESC(PASSWORD, "password", "Management Password", str),
  NSDP_PROPERTY_DESC(DHCP, "dhcp", "Use DHCP", u8),
  NSDP_PROPERTY_DESC(FIRMWARE_VERSION, "firmware-version",
                     "Firmware Version", str),
  NSDP_PROPERTY_DESC(PORT_STATUS, "port-status", "Port Status", port_status),
  NSDP_PROPERTY_DESC(PORT_COUNT, "port-count", "Port Count", u8),
};

const struct nsdp_property_desc*
  nsdp_get_property_desc_from_tag(nsdp_tag_t tag)
{
  int i;

  for (i = 0 ; i < ARRAY_SIZE(nsdp_property_desc) ; i += 1)
    if (nsdp_property_desc[i].tag == tag)
      return &nsdp_property_desc[i];

  return NULL;
}

const struct nsdp_property_desc*
  nsdp_get_property_desc_from_name(const char *name)
{
  int i;

  if (!name || !name[0])
      return NULL;

  for (i = 0 ; i < ARRAY_SIZE(nsdp_property_desc) ; i += 1)
    if (!strcmp(name, nsdp_property_desc[i].name))
      return &nsdp_property_desc[i];

  return NULL;
}

const struct nsdp_property_desc*
  nsdp_get_property_desc(const char *name)
{
  if (!name || !name[0])
      return NULL;

  return isdigit(name[0]) ?
    nsdp_get_property_desc_from_tag(strtol(name, NULL, 0)) :
    nsdp_get_property_desc_from_name(name);
}
