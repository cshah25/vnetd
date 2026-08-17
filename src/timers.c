#include "timers.h"
#include <time.h>
#include <stddef.h>

uint64_t current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void process_timers(void) {
    // TODO: Send keepalive packets for inactive peers
}
