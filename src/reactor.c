#include "reactor.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_EVENTS 64

static int epoll_fd = -1;

typedef struct {
    int fd;
    event_cb_t cb;
    void *arg;
} event_data_t;

int reactor_init(void) {
    epoll_fd = epoll_create1(0);
    return epoll_fd < 0 ? -1 : 0;
}

void reactor_destroy(void) {
    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }
}

int reactor_add(int fd, uint32_t events, event_cb_t cb, void *arg) {
    if (epoll_fd < 0) return -1;

    event_data_t *data = malloc(sizeof(event_data_t));
    if (!data) return -1;
    data->fd = fd;
    data->cb = cb;
    data->arg = arg;

    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = data;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        free(data);
        return -1;
    }
    return 0;
}

int reactor_run(void) {
    if (epoll_fd < 0) return -1;
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            event_data_t *data = events[i].data.ptr;
            if (data && data->cb) {
                data->cb(data->fd, events[i].events, data->arg);
            }
        }
    }
    return 0;
}
