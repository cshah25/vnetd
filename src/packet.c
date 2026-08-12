#include "packet.h"

int is_ipv4(const uint8_t *packet, size_t len) {
    if (len < 20) return 0; // Minimum IPv4 header length
    uint8_t version = packet[0] >> 4;
    return version == 4;
}
