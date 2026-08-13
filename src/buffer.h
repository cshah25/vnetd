#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stddef.h>

#define VPN_MTU 1500

typedef struct {
    uint8_t data[VPN_MTU];
    size_t length;
} vpn_buffer_t;

// Pool initialization
int buffer_pool_init(size_t pool_size);
void buffer_pool_destroy(void);

// Get/release buffers
vpn_buffer_t *buffer_get(void);
void buffer_release(vpn_buffer_t *buf);

#endif // BUFFER_H
