#include "crypto.h"
#include <sodium.h>

int crypto_init(void) {
    if (sodium_init() < 0) {
        return -1;
    }
    return 0;
}

int crypto_encrypt(
    uint8_t *payload, size_t payload_len,
    const uint8_t *ad, size_t ad_len,
    const uint8_t *nonce,
    const uint8_t *key
) {
    // libsodium requires a 12-byte nonce (IETF). Our protocol header only has 8 bytes.
    // We will pad the 8-byte nonce with 4 bytes of zero.
    uint8_t ietf_nonce[12] = {0};
    for (int i = 0; i < 8; i++) {
        ietf_nonce[4 + i] = nonce[i];
    }

    unsigned long long ciphertext_len;
    int res = crypto_aead_chacha20poly1305_ietf_encrypt(
        payload, &ciphertext_len,
        payload, payload_len,
        ad, ad_len,
        NULL, ietf_nonce, key
    );

    if (res != 0) return -1;
    return (int)ciphertext_len;
}

int crypto_decrypt(
    uint8_t *ciphertext, size_t ciphertext_len,
    const uint8_t *ad, size_t ad_len,
    const uint8_t *nonce,
    const uint8_t *key
) {
    if (ciphertext_len < CRYPTO_MAC_LEN) return -1;

    uint8_t ietf_nonce[12] = {0};
    for (int i = 0; i < 8; i++) {
        ietf_nonce[4 + i] = nonce[i];
    }

    unsigned long long decrypted_len;
    int res = crypto_aead_chacha20poly1305_ietf_decrypt(
        ciphertext, &decrypted_len,
        NULL,
        ciphertext, ciphertext_len,
        ad, ad_len,
        ietf_nonce, key
    );

    if (res != 0) return -1;
    return (int)decrypted_len;
}
