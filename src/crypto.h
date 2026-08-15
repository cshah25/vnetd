#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

#define CRYPTO_KEY_LEN 32
#define CRYPTO_MAC_LEN 16 // Poly1305 MAC

int crypto_init(void);

// Encrypt payload in place. Output buffer must have CRYPTO_MAC_LEN extra bytes.
// Returns new length on success, -1 on failure.
int crypto_encrypt(
    uint8_t *payload, size_t payload_len,
    const uint8_t *ad, size_t ad_len,
    const uint8_t *nonce,
    const uint8_t *key
);

// Decrypt payload in place. 
// Returns original plaintext length on success, -1 on failure.
int crypto_decrypt(
    uint8_t *ciphertext, size_t ciphertext_len,
    const uint8_t *ad, size_t ad_len,
    const uint8_t *nonce,
    const uint8_t *key
);

#endif // CRYPTO_H
