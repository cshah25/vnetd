#include "replay.h"

void replay_window_init(replay_window_t *window) {
    window->last_rx_seq = 0;
    window->replay_bitmap = 0;
}

bool replay_window_check(replay_window_t *window, uint32_t seq) {
    if (seq == 0) return false; // Invalid sequence number

    if (seq > window->last_rx_seq) {
        // Packet is newer than window upper bound
        uint32_t diff = seq - window->last_rx_seq;
        if (diff < 64) {
            window->replay_bitmap <<= diff;
            window->replay_bitmap |= 1ULL;
        } else {
            window->replay_bitmap = 1ULL;
        }
        window->last_rx_seq = seq;
        return true;
    }

    uint32_t diff = window->last_rx_seq - seq;
    if (diff >= 64) {
        // Packet is too old (outside window)
        return false;
    }

    if (window->replay_bitmap & (1ULL << diff)) {
        // Duplicate packet detected
        return false;
    }

    // Mark sequence number as received
    window->replay_bitmap |= (1ULL << diff);
    return true;
}
