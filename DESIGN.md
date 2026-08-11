# Technical Design: `vnetd`

*(This document is continuously updated to reflect ongoing architectural decisions and data structures.)*

## Architecture Overview
`vnetd` uses a single-threaded asynchronous event loop (`epoll`) to multiplex between a local `tun` interface and a `udp` WAN socket. 

## Protocol Header
All encrypted packets have a 16-byte unencrypted header:
- Type (1 byte)
- Reserved (1 byte)
- Session ID (2 bytes)
- Sequence Number (4 bytes)
- Nonce / IV (8 bytes)

## Memory Management
Memory allocations for incoming and outgoing packets will be managed via pre-allocated ring buffers aligned to cache-lines (64-byte alignment) to avoid runtime `malloc()` and `free()`.
