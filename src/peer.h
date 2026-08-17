#ifndef PEER_H
#define PEER_H

#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>
#include "crypto.h"
#include "replay.h"

typedef struct {
    uint16_t session_id;
    
    // Cryptographic state
    uint8_t static_public_key[CRYPTO_KEY_LEN];
    uint8_t symmetric_rx_key[CRYPTO_KEY_LEN];
    uint8_t symmetric_tx_key[CRYPTO_KEY_LEN];
    
    // Anti-replay state
    replay_window_t replay_window;
    uint32_t tx_seq;
    
    // Endpoint tracking (for dynamic roaming)
    struct sockaddr_in remote_addr;
    bool is_connected;
    
    // Allowed IPs
    uint32_t allowed_ip;
    uint32_t allowed_mask;
    
    // Timers
    uint64_t last_seen_timestamp;
    uint64_t last_sent_timestamp;
} vpn_peer_t;

int peer_table_init(size_t max_peers);
vpn_peer_t *peer_add(uint16_t session_id, uint32_t allowed_ip, uint32_t allowed_mask);
vpn_peer_t *peer_find_by_session(uint16_t session_id);
vpn_peer_t *peer_find_by_ip(uint32_t dest_ip);

#endif // PEER_H
