#ifndef PROTOCOL_H
#define PROTOCOL_H

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

#endif // PROTOCOL_H
