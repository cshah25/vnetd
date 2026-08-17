#include "peer.h"
#include <stdlib.h>
#include <string.h>

static vpn_peer_t **peer_table = NULL;
static size_t table_capacity = 0;
static size_t peer_count = 0;

int peer_table_init(size_t max_peers) {
    if (peer_table) return -1;
    peer_table = calloc(max_peers, sizeof(vpn_peer_t *));
    if (!peer_table) return -1;
    table_capacity = max_peers;
    peer_count = 0;
    return 0;
}

vpn_peer_t *peer_add(uint16_t session_id, uint32_t allowed_ip, uint32_t allowed_mask) {
    if (peer_count >= table_capacity) return NULL;
    
    if (peer_find_by_session(session_id)) return NULL;

    vpn_peer_t *peer = calloc(1, sizeof(vpn_peer_t));
    if (!peer) return NULL;

    peer->session_id = session_id;
    peer->allowed_ip = allowed_ip;
    peer->allowed_mask = allowed_mask;
    replay_window_init(&peer->replay_window);
    peer->tx_seq = 1;
    peer->is_connected = false;
    
    peer_table[peer_count++] = peer;
    return peer;
}

vpn_peer_t *peer_find_by_session(uint16_t session_id) {
    for (size_t i = 0; i < peer_count; i++) {
        if (peer_table[i]->session_id == session_id) {
            return peer_table[i];
        }
    }
    return NULL;
}

vpn_peer_t *peer_find_by_ip(uint32_t dest_ip) {
    for (size_t i = 0; i < peer_count; i++) {
        if ((dest_ip & peer_table[i]->allowed_mask) == (peer_table[i]->allowed_ip & peer_table[i]->allowed_mask)) {
            return peer_table[i];
        }
    }
    return NULL;
}
