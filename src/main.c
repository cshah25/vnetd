#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "tun.h"
#include "udp.h"
#include "packet.h"
#include "buffer.h"
#include "reactor.h"
#include "mtu.h"
#include "crypto.h"
#include "protocol.h"

#define BIND_PORT 8200
#define TCP_MSS 1360

static int tun_fd = -1;
static int udp_fd = -1;
static struct sockaddr_in remote_addr;

static void on_tun_read(int fd, uint32_t events, void *arg) {
    (void)events;
    (void)arg;
    
    vpn_buffer_t *buf = buffer_get();
    if (!buf) return;
    
    ssize_t n = read(fd, buf->data, VPN_MTU);
    if (n > 0) {
        buf->length = n;
        if (is_ipv4(buf->data, buf->length)) {
            clamp_tcp_mss(buf->data, buf->length, TCP_MSS);
            if (remote_addr.sin_port != 0) {
                sendto(udp_fd, buf->data, buf->length, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
                printf("[TUN -> UDP] Encapsulated and forwarded %zu bytes\n", buf->length);
            }
        }
    }
    buffer_release(buf);
}

static void on_udp_read(int fd, uint32_t events, void *arg) {
    (void)events;
    (void)arg;
    
    vpn_buffer_t *buf = buffer_get();
    if (!buf) return;
    
    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    ssize_t n = recvfrom(fd, buf->data, VPN_MTU, 0, (struct sockaddr *)&src_addr, &src_len);
    if (n > 0) {
        buf->length = n;
        if (write(tun_fd, buf->data, buf->length) < 0) {
            perror("write tun_fd");
        } else {
            printf("[UDP -> TUN] Decapsulated and forwarded %zu bytes\n", buf->length);
        }
    }
    buffer_release(buf);
}

int main(int argc, char *argv[]) {
    char tun_name[16] = "tun0";
    if (argc > 1) {
        strncpy(tun_name, argv[1], sizeof(tun_name) - 1);
        tun_name[sizeof(tun_name) - 1] = '\0';
    }

    if (buffer_pool_init(1024) < 0) {
        fprintf(stderr, "Failed to initialize buffer pool\n");
        return 1;
    }

    if (crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium crypto\n");
        return 1;
    }

    tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) {
        fprintf(stderr, "Failed to allocate TUN interface %s\n", tun_name);
        return 1;
    }
    printf("Successfully allocated TUN interface %s\n", tun_name);

    int bind_port = BIND_PORT;
    if (argc > 2) {
        bind_port = atoi(argv[2]);
    }

    udp_fd = udp_bind(bind_port);
    if (udp_fd < 0) {
        fprintf(stderr, "Failed to bind UDP port %d\n", bind_port);
        return 1;
    }
    printf("Successfully bound UDP port %d\n", bind_port);

    memset(&remote_addr, 0, sizeof(remote_addr));
    if (argc > 4) {
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_addr.s_addr = inet_addr(argv[3]);
        remote_addr.sin_port = htons(atoi(argv[4]));
        printf("Configured remote peer: %s:%s\n", argv[3], argv[4]);
    }

    if (reactor_init() < 0) {
        fprintf(stderr, "Failed to initialize reactor\n");
        return 1;
    }

    reactor_add(tun_fd, EPOLLIN, on_tun_read, NULL);
    reactor_add(udp_fd, EPOLLIN, on_udp_read, NULL);

    printf("Starting reactor event loop (Phase 2)...\n");
    reactor_run();

    reactor_destroy();
    close(tun_fd);
    close(udp_fd);
    buffer_pool_destroy();
    return 0;
}
