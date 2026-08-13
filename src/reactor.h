#ifndef REACTOR_H
#define REACTOR_H

#include <stdint.h>
#include <sys/epoll.h>

typedef void (*event_cb_t)(int fd, uint32_t events, void *arg);

int reactor_init(void);
void reactor_destroy(void);
int reactor_add(int fd, uint32_t events, event_cb_t cb, void *arg);
int reactor_run(void);

#endif // REACTOR_H
