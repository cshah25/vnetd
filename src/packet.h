#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stddef.h>

// Parse IP packet to determine if it is IPv4/IPv6 and extract simple metadata.
// For Phase 1, just a dummy check.
int is_ipv4(const uint8_t *packet, size_t len);

#endif // PACKET_H
