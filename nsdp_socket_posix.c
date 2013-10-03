#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "nsdp_socket.h"

int nsdp_socket_open(const char* dev, const char* local_addr,
                     int local_port, nsdp_socket_t* sock)
{
  int err = 0, broadcast = 1, fd;
  struct sockaddr_in addr = { .sin_family = AF_INET,
                              .sin_addr.s_addr = INADDR_ANY,
                              .sin_port = htons(local_port) };
  // Create the socket
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return -errno;

  // Allow sending broadcast packets
  err = setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                   &broadcast, sizeof(broadcast));
  if (err)
    fprintf(stderr, "Failed to set broadcast mode: %s\n", strerror(errno));

  // If a device is given, bind to it
  if (dev) {
#ifdef SO_BINDTODEVICE
    struct ifreq ifr = {};
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", dev);
    err = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
                     &ifr, sizeof(ifr));
    if (err < 0) {
      err = -errno;
      fprintf(stderr, "Failed to bind to device %s: %s\n",
              dev, strerror(errno));
      return err;
    }
#endif
  }

  // If a local address is given, use it instead of INADDR_ANY
  if (local_addr) {
    err = inet_aton(local_addr, &addr.sin_addr);
    if (err < 0) {
      err = -EINVAL;
      goto error;
    }
  }

  // Bind the socket to the local address
  err = bind(fd,(const struct sockaddr*) &addr, sizeof(addr));
  if (err) {
    err = -errno;
    fprintf(stderr, "Failed to bind socket to addess %s: %s\n",
            local_addr ? local_addr : "ANY", strerror(errno));
    goto error;
  }

  if (sock)
    *sock = fd;
  else
    close(fd);

  return 0;

 error:
  close(fd);
  return err;
}

int nsdp_socket_close(nsdp_socket_t sock)
{
  return close(sock);
}

int nsdp_socket_sendto(nsdp_socket_t sock, const void *buf,
                       unsigned length, const nsdp_socket_addr_t *to)
{
  return sendto(sock, buf, length, 0,
                (const struct sockaddr*)to, to ? sizeof(*to) : 0);
}

int nsdp_socket_recvfrm(nsdp_socket_t sock, void *buf,
                        unsigned length, nsdp_socket_addr_t *from)
{
  socklen_t slen = sizeof(*from);
  return recvfrom(sock, buf, length, 0,
                  (struct sockaddr*)from, from ? &slen : NULL);
}

int nsdp_socket_addr_aton(nsdp_socket_addr_t* addr, const char* ip)
{
  if (!addr || !ip)
    return -EINVAL;

  addr->sin_family = AF_INET;
  if (!inet_aton(ip, &addr->sin_addr))
    return -EINVAL;

  return 0;
}

int nsdp_socket_addr_set_broadcast(nsdp_socket_addr_t* addr)
{
  if (!addr)
    return -EINVAL;

  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_BROADCAST;

  return 0;
}

int nsdp_socket_addr_set_anyaddr(nsdp_socket_addr_t* addr)
{
  if (!addr)
    return -EINVAL;

  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;

  return 0;
}

int nsdp_socket_addr_set_port(nsdp_socket_addr_t* addr, unsigned port)
{
  if (!addr)
    return -EINVAL;

  addr->sin_port = htons(port);

  return 0;
}

