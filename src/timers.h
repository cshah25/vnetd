#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>
#include "peer.h"

uint64_t current_time_ms(void);
void process_timers(void);

#endif // TIMERS_H
