# TODO: Userspace L3 VPN Daemon (`vnetd`)

## Priority 1: Core Networking Foundation
- [x] Setup CMake build system
- [x] Implement TUN interface driver (`tun.c` / `tun.h`)
- [x] Implement UDP socket setup (`udp.c` / `udp.h`)
- [x] Build raw IP packet parser (`packet.c` / `packet.h`)
- [x] Set up daemon entry point (`main.c`) for unencrypted forwarding loop

## Priority 2: Epoll Asynchronous Engine
- [x] Build `epoll` reactor loop (`reactor.c` / `reactor.h`)
- [x] Implement memory pooling (`buffer.c` / `buffer.h`)
- [x] Write TCP MSS clamping utility (`mtu.c` / `mtu.h`)

## Priority 3: Cryptography & Security Subsystem
- [ ] Wrap `libsodium` AEAD (`crypto.c` / `crypto.h`)
- [ ] Define binary wire protocol (`protocol.h`)
- [ ] Implement sliding-window anti-replay (`replay.c` / `replay.h`)

## Priority 4: Session & Control Plane
- [ ] Build peer tracking (`peer.c` / `peer.h`)
- [ ] Implement Noise-like handshake (`handshake.c` / `handshake.h`)
- [ ] Add keepalive heartbeats (`timers.c` / `timers.h`)
- [ ] Dynamic remote IP tracking

## Priority 5: Routing & System Integration
- [ ] Routing utilities (`route.c` / `system.c`)
- [ ] `SO_MARK` socket options
- [ ] Setup and teardown scripts
