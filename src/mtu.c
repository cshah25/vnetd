#include "mtu.h"
#include <netinet/in.h>

// Stub implementation for Phase 2
int clamp_tcp_mss(uint8_t *packet, size_t len, uint16_t max_mss) {
    (void)packet;
    (void)len;
    (void)max_mss;
    // TODO: Parse IP header, check protocol is TCP, find TCP header, find MSS option,
    // modify it, and recalculate TCP checksum.
    return 0;
}
