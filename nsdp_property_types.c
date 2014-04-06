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
  int val;

  if (data_size == 0)
    return 1;

  val = atoi(txt);
  if(val < 0 || val > 0xFF)
    return -EINVAL;

  ((uint8_t*)data)[0] = val;
  return 1;
}

static int nsdp_read_u8_property(const void *data, unsigned data_size,
                                 char* txt, unsigned txt_size)
{
  int val;

  if (data_size != 1)
    return -E2BIG;

  val = ((uint8_t*)data)[0];
  snprintf(txt, txt_size, "%d", val);
  return 1;
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
