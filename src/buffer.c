#include "buffer.h"
#include <stdlib.h>
#include <stdio.h>

static vpn_buffer_t **pool = NULL;
static size_t pool_capacity = 0;
static size_t available_count = 0;

int buffer_pool_init(size_t pool_size) {
    if (pool != NULL) return -1;
    
    pool = malloc(sizeof(vpn_buffer_t *) * pool_size);
    if (!pool) return -1;

    pool_capacity = pool_size;
    available_count = pool_size;

    for (size_t i = 0; i < pool_size; i++) {
        if (posix_memalign((void **)&pool[i], 64, sizeof(vpn_buffer_t)) != 0) {
            return -1;
        }
    }
    
    return 0;
}

void buffer_pool_destroy(void) {
    if (!pool) return;
    for (size_t i = 0; i < pool_capacity; i++) {
        free(pool[i]);
    }
    free(pool);
    pool = NULL;
    pool_capacity = 0;
    available_count = 0;
}

vpn_buffer_t *buffer_get(void) {
    if (available_count == 0) return NULL;
    vpn_buffer_t *buf = pool[--available_count];
    buf->length = 0;
    return buf;
}

void buffer_release(vpn_buffer_t *buf) {
    if (!pool || !buf || available_count >= pool_capacity) return;
    pool[available_count++] = buf;
}
