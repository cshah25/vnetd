# Technical Design Document: Userspace L3 VPN Daemon (`vnetd`)

---

## 1. System Overview & Goals

`vnetd` is a lightweight, high-performance userspace Virtual Private Network (VPN) daemon written in C/C++. It establishes a secure, point-to-point Layer 3 (IP) overlay tunnel over untrusted UDP networks using authenticated encryption (AEAD).

```
   +-----------------------------------------------------------------------+
   |                             HOST SYSTEM                               |
   |                                                                       |
   |  +------------------+         IP Route         +-------------------+  |
   |  | User Application | ------------------------>|  Kernel Network   |  |
   |  | (Browser/SSH/etc)|                          |      Stack        |  |
   |  +------------------+                          +---------+---------+  |
   |                                                          |            |
   |                                                   /dev/net/tun        |
   |                                                          |            |
   |  +-------------------------------------------------------v---------+  |
   |  |                         USERS-SPACE                             |  |
   |  |                        `vnetd` DAEMON                           |  |
   |  |                                                                 |  |
   |  |  +-----------------+    +----------------+    +--------------+  |  |
   |  |  | Epoll Reactor   |--->| Session & Peer |--->| Cryptographic|  |  |
   |  |  | Event Loop      |    | Table          |    | Engine (AEAD)|  |  |
   |  |  +-----------------+    +----------------+    +--------------+  |  |
   |  |                                                         |       |  |
   |  +---------------------------------------------------------|-------+  |
   |                                                            |          |
   |                                                     UDP Socket        |
   +------------------------------------------------------------|----------+
                                                                v
                                                          Public WAN

```

### Key Engineering Objectives

* **Zero-Trust Encapsulation:** Encrypt and authenticate all Layer 3 payloads using ChaCha20-Poly1305.
* **Low Latency & High Throughput:** Non-blocking asynchronous I/O driven by Linux `epoll`.
* **Dynamic Roaming:** Support seamless client mobility across IP/port changes without dropping sessions.
* **Minimal Dependencies:** Built on POSIX APIs, standard Linux kernel interfaces (`/dev/net/tun`), and `libsodium`.

---

## 2. Architecture & Wire Protocol Specification

### 2.1 Wire Format Design

To minimize packet expansion while providing cryptographic security and replay protection, all datagrams transmitted over UDP use a fixed 16-byte header followed by the encrypted IP payload.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Type (1B)    | Reserved (1B) |          Session ID (2B)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Sequence Number (4B)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Nonce / IV (8B)                        |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Encrypted Payload + Auth Tag                |
|                             ...                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

```

#### C Header Wire Structure

```c
#include <stdint.h>

#define VPN_PROTO_MAGIC 0x56 // 'V'

typedef enum {
    MSG_HANDSHAKE_INIT = 0x01,
    MSG_HANDSHAKE_RESP = 0x02,
    MSG_DATA           = 0x03,
    MSG_KEEPALIVE      = 0x04
} msg_type_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;          // Packet type discriminator
    uint8_t  reserved;      // Alignment padding / future flags
    uint16_t session_id;    // Unique peer session identifier
    uint32_t seq_num;       // Monotonically increasing sequence number
    uint8_t  nonce[8];      // Initialization Vector for AEAD
} vpn_header_t;
#pragma pack(pop)

```

### 2.2 Peer State Machine & Control Plane

Each active peer is tracked in an internal hash table indexed by `Session ID` and `Allowed IP` ranges.

```
            +---------------+
            |  UNINITIATED  |
            +-------+-------+
                    |
           Send Handshake Init
                    v
            +---------------+
            |   HANDSHAKE   |
            |   SENT/RCVD   |
            +-------+-------+
                    |
         Verify Noise Handshake
                    v
            +---------------+
            |  ESTABLISHED  | <---+ (Valid Data / Keepalive)
            +-------+-------+     |
                    |             |
             Inactivity Timeout   |
              (e.g., 180s) -------+
                    v
            +---------------+
            |    EXPIRED    |
            +---------------+

```

---

## 3. Deep-Dive Technical Challenges & Engineering Solutions

### Challenge 1: Avoid Routing Loops (The "Default Gateway" Problem)

* **The Problem:** When you configure the OS routing table to direct all traffic (`0.0.0.0/0`) into the `tun0` interface, the VPN daemon's own encrypted UDP transport packets will also get routed into `tun0`. This causes an infinite recursion loop that freezes the network stack.
* **Engineering Solution:**
1. **Socket Marking (`SO_MARK`):** Bind the daemon's WAN UDP socket to a specific firewall mark:
```c
int mark = 0x42;
setsockopt(udp_fd, SOL_SOCKET, SO_MARK, &mark, sizeof(mark));

```


2. **Policy Routing (`ip rule`):** Configure the kernel routing rules so marked packets bypass the default TUN route and use the physical interface's routing table directly:
```bash
ip rule add fwmark 0x42 lookup 257
ip route add default via <PHYSICAL_GW_IP> dev eth0 table 257

```





---

### Challenge 2: Path MTU (PMTU), MSS Clamping, & Fragmentation

* **The Problem:** Encapsulating an IP packet inside a UDP frame adds header overhead:

$$\text{Overhead} = \text{IP Header (20B)} + \text{UDP Header (8B)} + \text{VPN Header (16B)} + \text{Poly1305 Tag (16B)} = 60\text{ bytes}$$



If a client app sends a standard 1500-byte IP packet, encapsulation increases its size to 1560 bytes. The physical WAN interface will either drop the packet (if DF bit is set) or fragment it, degrading performance.
* **Engineering Solution:**
1. **TUN MTU Reduction:** Set the TUN interface MTU to $1500 - 60 = 1440$ bytes:
```bash
ip link set dev vpntun0 mtu 1440

```


2. **TCP MSS Clamping:** Intercept TCP `SYN` packets inside the daemon or via `iptables` to rewrite the Maximum Segment Size (MSS) option in the TCP header to match the reduced payload capability:
```bash
iptables -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

```





---

### Challenge 3: Out-of-Order UDP & Sliding-Window Anti-Replay Defense

* **The Problem:** UDP provides no packet ordering guarantees. An attacker monitoring the public WAN could intercept an encrypted valid packet and re-transmit it millions of times (Replay Attack) to overwhelm internal systems.
* **Engineering Solution:** Implement a **Sliding Window Anti-Replay Algorithm** using a 64-bit sliding bitmap and a tracking sequence number ($S_{\text{max}}$).

```
                        Sliding Window (Size = 64 bits)
              [S_max - 63]  <------------------->  [S_max]
Bit Index:        63                                  0
Bitmap State:    [ 1 | 1 | 0 | 1 | 1 | ... | 1 | 0 | 1 ]
                                    ^
                              Received Packet

```

#### Verification Logic:

1. **Case A (Packet $S > S_{\text{max}}$):** The packet is newer than any seen before. Shift the 64-bit mask left by $(S - S_{\text{max}})$, set bit 0 to `1`, and update $S_{\text{max}} = S$.
2. **Case B ($S \le S_{\text{max}}$ and $S > S_{\text{max}} - 64$):** The packet falls inside the window. Check bit position $(S_{\text{max}} - S)$.
* If bit is `1`: **Reject (Duplicate replay detected)**.
* If bit is `0`: **Accept**, set bit to `1`.


3. **Case C ($S \le S_{\text{max}} - 64$):** The packet is too old. **Reject immediately**.

---

### Challenge 4: Key Exchange & Perfect Forward Secrecy (PFS)

* **The Problem:** Hardcoding static symmetric keys means if a key is ever compromised, all past intercepted traffic can be decrypted retroactively.
* **Engineering Solution:** Implement an Ephemeral-Static Elliptic-Curve Diffie-Hellman (ECDH) key exchange pattern based on the **Noise Protocol Framework** (specifically `Noise_IKpsk2` or `Curve25519`).
* **Static Keys:** Used for long-term peer identity verification.
* **Ephemeral Keys:** Fresh keys generated per session handshake.
* **Rekey Timers:** Automate session key renegotiation every $N$ gigabytes of transferred data or every $T$ minutes (e.g., 120 seconds).



---

### Challenge 5: Dynamic Client NAT Traversal & Endpoint Roaming

* **The Problem:** Mobile clients frequently change IP addresses (e.g., switching from Wi-Fi to cellular networks) and sit behind NAT devices that close UDP mapping tables after short periods of inactivity.
* **Engineering Solution:**
1. **Dynamic Remote Updating:** When the server receives a valid, cryptographically authenticated data packet from a peer, it updates that peer's registered remote IP and port in the session table to match the UDP packet's source socket address (`recvfrom`).
2. **Active Keepalive Heartbeats:** If no data packets are sent across the tunnel for 25 seconds, the daemon emits an encrypted 0-byte payload (`MSG_KEEPALIVE`) to refresh the NAT state tables on intermediate routers.



---

### Challenge 6: Zero-Copy Mechanics & Userspace Context-Switch Overhead

* **The Problem:** Copying packet buffers repeatedly between kernel network memory and userspace applications adds CPU overhead and increases latency at gigabit throughput.
* **Engineering Solution:**
* Use **ring buffers** for memory allocations.
* Allocate memory using `posix_memalign` aligned to cache line boundaries (64 bytes).
* Configure Linux socket buffers (`SO_RCVBUF`, `SO_SNDBUF`) to high limits (e.g., 4MB) to absorb burst traffic during event-loop processing spikes.



---

## 4. Implementation Plan & Execution Roadmap

The implementation is broken down into five distinct engineering phases to allow systematic building and testing.

```
+-----------------------------------------------------------------------------------+
|                        PHASE 1: Core Networking Foundation                        |
| - Implement TUN interface driver (/dev/net/tun wrapper)                           |
| - Build raw IP packet parser (extract IPv4/IPv6 headers)                          |
| - Set up unencrypted UDP tunnel between two netns                                 |
+-----------------------------------------------------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                       PHASE 2: Epoll Asynchronous Engine                          |
| - Build non-blocking reactor loop monitoring tun_fd and sock_fd                   |
| - Implement buffer pooling to eliminate runtime malloc/free                       |
| - Write dynamic MTU/MSS clamping utilities                                        |
+-----------------------------------------------------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                      PHASE 3: Cryptography & Security Subsystem                   |
| - Integrate libsodium (crypto_aead_chacha20poly1305)                              |
| - Build binary wire protocol framing & packet serialization                       |
| - Implement sliding-window anti-replay mechanism                                  |
+-----------------------------------------------------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                       PHASE 4: Session & Control Plane                            |
| - Build peer tracking table (allowed IPs <-> Session IDs)                         |
| - Implement Noise-like Ephemeral Diffie-Hellman Handshake                         |
| - Add active Keepalive heartbeats & dynamic peer IP roaming                       |
+-----------------------------------------------------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                   PHASE 5: Routing, System Integration & Stress                   |
| - Automate Linux routing table & iptables policy setup                             |
| - Implement socket marking (SO_MARK) to break routing loops                       |
| - Run load testing using iperf3 across the virtual tunnel                         |
+-----------------------------------------------------------------------------------+

```

---

## 5. Core Data Structures & Code Architecture

### 5.1 Peer State Structure (`peer.h`)

```c
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>

#define KEY_LEN 32

typedef struct {
    uint16_t session_id;
    
    // Cryptographic state
    uint8_t static_public_key[KEY_LEN];
    uint8_t symmetric_rx_key[KEY_LEN];
    uint8_t symmetric_tx_key[KEY_LEN];
    
    // Anti-replay state
    uint32_t last_rx_seq;
    uint64_t replay_bitmap;
    uint32_t tx_seq;
    
    // Endpoint tracking (for dynamic roaming)
    struct sockaddr_in remote_addr;
    bool is_connected;
    
    // Timers
    uint64_t last_seen_timestamp;
    uint64_t last_sent_timestamp;
} vpn_peer_t;

```

### 5.2 Anti-Replay Verification Algorithm (`replay.c`)

```c
#include <stdint.h>
#include <stdbool.h>

bool check_and_update_replay_window(vpn_peer_t *peer, uint32_t seq) {
    if (seq == 0) return false; // Invalid sequence number

    if (seq > peer->last_rx_seq) {
        // Packet is newer than window upper bound
        uint32_t diff = seq - peer->last_rx_seq;
        if (diff < 64) {
            peer->replay_bitmap <<= diff;
            peer->replay_bitmap |= 1ULL;
        } else {
            peer->replay_bitmap = 1ULL;
        }
        peer->last_rx_seq = seq;
        return true;
    }

    uint32_t diff = peer->last_rx_seq - seq;
    if (diff >= 64) {
        // Packet is too old (outside window)
        return false;
    }

    if (peer->replay_bitmap & (1ULL << diff)) {
        // Duplicate packet detected
        return false;
    }

    // Mark sequence number as received
    peer->replay_bitmap |= (1ULL << diff);
    return true;
}

```

---

## 6. Deep-Dive Research & Reference Matrix

To deepen your research into building low-level userspace tunnels, consult the following standard RFCs and kernel documentation:

| Domain | Standard / Topic | Key Concepts to Focus On |
| --- | --- | --- |
| **Tunneling Interface** | Linux Universal TUN/TAP Driver | `/dev/net/tun`, `TUNSETIFF`, `IFF_TUN`, `IFF_NO_PI` |
| **WireGuard Spec** | Jason A. Donenfeld (2017 Paper) | Noise Protocol Framework, Stealth Protocol Design, Cookie-based Handshakes |
| **Cryptography** | RFC 8439 | ChaCha20 and Poly1305 for IETF Protocols |
| **Key Exchange** | Noise Protocol Framework Spec | `Noise_IK` Pattern (1-RTT handshake with mutual authentication) |
| **Path MTU** | RFC 1191 / RFC 4821 | Path MTU Discovery (PMTUD) and Packet Too Big ICMP messages |
| **Anti-Replay** | RFC 4303 (IPsec ESP) | Section 3.4.3: Extended Sequence Number (ESN) Processing |
| **High Performance I/O** | Linux `epoll(7)` / `io_uring(7)` | Edge-Triggered vs. Level-Triggered I/O, zero-copy system calls (`splice`) |