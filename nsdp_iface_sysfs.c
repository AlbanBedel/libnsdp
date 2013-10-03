#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "nsdp_socket.h"

#ifndef SYSFS_IFACE_ADDR
#define SYSFS_IFACE_ADDR "/sys/class/net/%s/address"
#endif

int nsdp_iface_get_mac(const char* iface, uint8_t mac[6])
{
  int err;
  char path[128];
  FILE* fd;
  snprintf(path, sizeof(path), SYSFS_IFACE_ADDR, iface);
  fd = fopen(path, "r");
  if (!fd)
    return -errno;
  err= fscanf(fd, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
              &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  fclose(fd);
  return err == 6 ? 0 : -EINVAL;
}
