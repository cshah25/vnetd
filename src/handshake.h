#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "peer.h"

int handshake_init_local(void);
const uint8_t *handshake_get_public_key(void);
int handshake_create_init(vpn_peer_t *peer, uint8_t *packet_out, size_t *out_len);
int handshake_process_packet(uint8_t *packet, size_t len, struct sockaddr_in *src_addr);

#endif // HANDSHAKE_H
