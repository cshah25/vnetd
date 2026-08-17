#include "handshake.h"
#include <sodium.h>
#include <string.h>

static uint8_t local_pk[crypto_kx_PUBLICKEYBYTES];
static uint8_t local_sk[crypto_kx_SECRETKEYBYTES];
static bool keys_initialized = false;

int handshake_init_local(void) {
    if (crypto_kx_keypair(local_pk, local_sk) != 0) return -1;
    keys_initialized = true;
    return 0;
}

const uint8_t *handshake_get_public_key(void) {
    return keys_initialized ? local_pk : NULL;
}

int handshake_create_init(vpn_peer_t *peer, uint8_t *packet_out, size_t *out_len) {
    (void)peer;
    (void)packet_out;
    (void)out_len;
    // TODO: Implement ECDH initiation
    return -1; 
}

int handshake_process_packet(uint8_t *packet, size_t len, struct sockaddr_in *src_addr) {
    (void)packet;
    (void)len;
    (void)src_addr;
    // TODO: Process ECDH initiation/response
    return -1;
}
