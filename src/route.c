#include "route.h"
#include <stdio.h>
#include <stdlib.h>

int route_add(uint32_t dest_ip, uint32_t mask, int ifindex) {
    (void)dest_ip;
    (void)mask;
    (void)ifindex;
    // TODO: Implement Netlink socket routing messages (RTM_NEWROUTE)
    return 0;
}
