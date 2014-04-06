#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nsdp_types.h"
#include "nsdp_property_types.h"

#define NSDP_PROPERTY_TYPE(name)                                        \
  nsdp_property_type_t nsdp_property_type_##name =                      \
  {                                                                     \
    .to_text = nsdp_read_##name##_property,                             \
    .from_text = nsdp_write_##name##_property,                          \
  }

#define NSDP_RO_PROPERTY_TYPE(name)                                     \
  nsdp_property_type_t nsdp_property_type_##name =                      \
  {                                                                     \
    .to_text = nsdp_read_##name##_property,                             \
    .from_text = nsdp_no_write_property,                                \
  }

#define NSDP_WO_PROPERTY_TYPE(name)                                     \
  nsdp_property_type_t nsdp_property_type_##name =                      \
  {                                                                     \
    .to_text = nsdp_no_read_property,                                   \
    .from_text = nsdp_write_##name##_property,                          \
  }

struct nsdp_enumeration {
  const char* name;
  int value;
};

static int nsdp_no_write_property(const char* txt, void *data,
                                   unsigned data_size)
{
  return -EINVAL;
}

static int nsdp_no_read_property(const void *data, unsigned data_size,
                                 char* txt, unsigned txt_size)
{
  return -EINVAL;
}

static int nsdp_enum_from_text(const struct nsdp_enumeration *enu,
                               const char* txt, int* val)
{
  int i;
  char* end;

  for (i = 0 ; enu && enu[i].name ; i += 1) {
    if (!strcmp(enu[i].name, txt)) {
      if (val)
        *val = enu[i].value;
      return 0;
    }
  }
  i = strtol(txt, &end, 0);
  if (val)
    *val = i;
  return end > txt ? 0 : -EINVAL;
}

static int nsdp_enum_to_text(const struct nsdp_enumeration *enu,
                              int val, char* txt, unsigned txt_size)
{
  int i;
  for (i = 0 ; enu && enu[i].name ; i += 1) {
    if (enu[i].value == val)
      return snprintf(txt, txt_size, "%s", enu[i].name);
  }
  return snprintf(txt, txt_size, "%d", val);
}

static int nsdp_write_u8_enum_property(const struct nsdp_enumeration *enu,
                                       const char* txt, void *data,
                                       unsigned data_size)
{
  int val, err;

  if (data_size == 0)
    return 1;

  err = nsdp_enum_from_text(enu, txt, &val);
  if (err)
    return err;

  if (val < 0 || val > 0xFF)
    return -EINVAL;

  ((uint8_t*)data)[0] = val;
  return 1;
}

static int nsdp_read_u8_enum_property(const struct nsdp_enumeration *enu,
                                      const void *data,
                                      unsigned data_size,
                                      char* txt, unsigned txt_size)
{
  if (data_size != 1)
    return -E2BIG;

  nsdp_enum_to_text(enu, ((uint8_t*)data)[0], txt, txt_size);
  return 1;
}

#define NSDP_ENUM_PROPERTY_WRITER(name, data_type, enum_desc)   \
  static int nsdp_write_##name##_property(const char* txt,      \
                                          void *data,           \
                                          unsigned data_size)   \
  {                                                             \
    return nsdp_write_##data_type##_enum_property(enum_desc,    \
                                                  txt,          \
                                                  data,         \
                                                  data_size);   \
  }

#define NSDP_ENUM_PROPERTY_READER(name, data_type, enum_desc)   \
  static int nsdp_read_##name##_property(const void *data,      \
                                         unsigned data_size,    \
                                         char* txt,             \
                                         unsigned txt_size)     \
  {                                                             \
    return nsdp_read_##data_type##_enum_property(enum_desc,     \
                                               data, data_size, \
                                               txt, txt_size);  \
  }

#define NSDP_ENUM_PROPERTY_TYPE(name, data_type)                \
  NSDP_ENUM_PROPERTY_WRITER(name, data_type, name##_enum)       \
  NSDP_ENUM_PROPERTY_READER(name, data_type, name##_enum)       \
  NSDP_PROPERTY_TYPE(name)

static int nsdp_write_str_property(const char* txt, void *data,
                                   unsigned data_size)
{
  int len = strlen(txt);

  if (data_size == 0)
    return len;

  if (data_size < len)
    return -E2BIG;

  memcpy(data, txt, len);
  return len;
}

static int nsdp_read_str_property(const void *data, unsigned data_size,
                                  char* txt, unsigned txt_size)
{
  if (txt_size < data_size+1)
    return -E2BIG;

  memcpy(txt, data, data_size);
  txt[data_size] = 0;
  return data_size;
}
NSDP_PROPERTY_TYPE(str);

static int nsdp_write_u8_property(const char* txt, void *data,
                                  unsigned data_size)
{
  return nsdp_write_u8_enum_property(NULL, txt, data, data_size);
}

static int nsdp_read_u8_property(const void *data, unsigned data_size,
                                 char* txt, unsigned txt_size)
{
  return nsdp_read_u8_enum_property(NULL, data, data_size, txt, txt_size);
}
NSDP_PROPERTY_TYPE(u8);

static int nsdp_write_ip4_property(const char* txt, void *data,
                                   unsigned data_size)
{
  struct in_addr *addr = data;

  if (data_size == 0)
    return sizeof(*addr);

  if (data_size < sizeof(*addr))
    return -E2BIG;

  if (!inet_aton(txt, addr))
    return -EINVAL;

  return sizeof(*addr);
}

static int nsdp_read_ip4_property(const void *data, unsigned data_size,
                                  char* txt, unsigned txt_size)
{
  const struct in_addr *addr = data;
  char *ip4;

  if (data_size != sizeof(*addr))
    return -EINVAL;

  ip4 = inet_ntoa(*addr);
  if (!ip4)
    return -EINVAL;

  snprintf(txt, txt_size, "%s", ip4);
  return sizeof(addr);
}
NSDP_PROPERTY_TYPE(ip4);

static int nsdp_write_mac_property(const char* txt, void *data,
                                   unsigned data_size)
{
  uint8_t *addr = data;

  if (data_size == 0)
    return sizeof(nsdp_mac_t);

  if (data_size < sizeof(nsdp_mac_t))
    return -E2BIG;

  sscanf(txt,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8
         ":%" SCNx8 ":%" SCNx8 ":%" SCNx8,
         &addr[0], &addr[1], &addr[2],
         &addr[3], &addr[4], &addr[5]);

  return sizeof(nsdp_mac_t);
}

static int nsdp_read_mac_property(const void *data, unsigned data_size,
                                  char* txt, unsigned txt_size)
{
  const uint8_t *addr = data;

  if (data_size != sizeof(nsdp_mac_t))
    return -EINVAL;

  snprintf(txt, txt_size,
           "%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
           ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8,
           addr[0], addr[1], addr[2],
           addr[3], addr[4], addr[5]);
  return sizeof(nsdp_mac_t);
}
NSDP_PROPERTY_TYPE(mac);

static int nsdp_write_port_status_property(const char* txt, void *data,
                                           unsigned data_size)
{
  unsigned port_id;
  char state[32];
  uint8_t* status = data;

  if (data_size == 0)
    return 3;

  if (data_size < 3)
    return -E2BIG;

  if (sscanf(txt, "%u:%31s", &port_id, state) != 2)
    return -EINVAL;

  // Port ID
  if (port_id > 0xFF)
    return -ERANGE;
  status[0] = port_id;

  // Port state
  if (!strcmp(state, "Disconnected") ||
      !strcmp(state, "disconnected") ||
      !strcmp(state, "Down") ||
      !strcmp(state, "down"))
    status[1] = 0;
  else if (!strcmp(state, "10M"))
    status[1] = 1;
  else if (!strcmp(state, "100M"))
    status[1] = 3;
  else if (!strcmp(state, "1000M") ||
           !strcmp(state, "1G"))
    status[1] = 5;
  else
    return -EINVAL;

  // Admin status?
  status[3] = 1;
  return 3;
}

static int nsdp_read_port_status_property(const void *data, unsigned data_size,
                                          char* txt, unsigned txt_size)
{
  const uint8_t* status = data;
  const char* state;

  if (data_size != 3)
    return -EINVAL;

  if (status[1] == 0)
    state = "Disconnected";
  else if (status[1] <= 2)
    state = "10M";
  else if (status[1] <= 4)
    state = "100M";
  else if (status[1] == 5)
    state = "1000M";
  else
    state = "Unknown";

  snprintf(txt, txt_size, "%d:%s", status[0], state);
  return 3;
}
NSDP_PROPERTY_TYPE(port_status);

static int nsdp_read_port_statistics_property(const void *data,
                                              unsigned data_size,
                                              char* txt, unsigned txt_size)
{
  const uint8_t *stat = data;

  if (data_size < (1 + 6*8))
    return -EINVAL;

  snprintf(txt, txt_size, "%d:rx=%" PRId64 ",tx=%" PRId64,
           stat[0], nsdp_get_u64be(stat+1), nsdp_get_u64be(stat+1+8));
  return 1 + 6*8;
}
NSDP_RO_PROPERTY_TYPE(port_statistics);

static const struct nsdp_enumeration vlan_engine_enum[] = {
  {
    .value = 1,
    .name  = "VLAN Port Based",
  },{
    .value = 2,
    .name  = "VLAN ID Based",
  },{
    .value = 3,
    .name  = "802.1Q Port Based",
  },{
    .value = 4,
    .name  = "802.1Q Extended",
  },{
  }
};
NSDP_ENUM_PROPERTY_TYPE(vlan_engine, u8);

static int nsdp_read_port_pvid_property(const void *data,
                                        unsigned data_size,
                                        char* txt, unsigned txt_size)
{
  const uint8_t *stat = data;

  if (data_size < 3)
    return -EINVAL;

  snprintf(txt, txt_size, "%d:%" PRId16,
           stat[0], nsdp_get_u16be(stat+1));
  return 3;
}
NSDP_RO_PROPERTY_TYPE(port_pvid);
