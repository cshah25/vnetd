#ifndef ROUTE_H
#define ROUTE_H

#include <stdint.h>

// Add a route to the system routing table via the given interface index
int route_add(uint32_t dest_ip, uint32_t mask, int ifindex);

#endif // ROUTE_H
