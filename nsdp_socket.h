#ifndef NSDP_SOCKET_H
#define NSDP_SOCKET_H

#include <stdint.h>
// Define the basic types
/// This header should be selected by configure
#include "nsdp_socket_posix.h"

// Should open a non blocking, broadcast capable socket
// bound to the given address and/or device
int nsdp_socket_open(const char* dev, const char* local_addr,
                     int local_port, nsdp_socket_t* sock);
int nsdp_socket_close(nsdp_socket_t sock);

int nsdp_socket_sendto(nsdp_socket_t sock, const void *buf,
                       unsigned length, const nsdp_socket_addr_t *to);

int nsdp_socket_recvfrom(nsdp_socket_t sock, void *buf,
                         unsigned length, nsdp_socket_addr_t *from);

int nsdp_socket_addr_aton(nsdp_socket_addr_t* addr, const char* ip);
int nsdp_socket_addr_set_broadcast(nsdp_socket_addr_t* addr);
int nsdp_socket_addr_set_anyaddr(nsdp_socket_addr_t* addr);
int nsdp_socket_addr_set_port(nsdp_socket_addr_t* addr, unsigned port);

int nsdp_iface_get_mac(const char* iface, uint8_t mac[6]);

#endif /* NSDP_SOCKET_H */
