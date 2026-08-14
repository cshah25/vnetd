#ifndef MTU_H
#define MTU_H

#include <stdint.h>
#include <stddef.h>

// Modifies the MSS option in the TCP SYN packet to the given max_mss.
// Returns 1 if modified, 0 otherwise.
int clamp_tcp_mss(uint8_t *packet, size_t len, uint16_t max_mss);

#endif // MTU_H
