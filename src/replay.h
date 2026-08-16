#ifndef REPLAY_H
#define REPLAY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t last_rx_seq;
    uint64_t replay_bitmap;
} replay_window_t;

void replay_window_init(replay_window_t *window);
bool replay_window_check(replay_window_t *window, uint32_t seq);

#endif // REPLAY_H
