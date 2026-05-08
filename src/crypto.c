#include "crypto.h"
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>
#include <openssl/err.h>

size_t aes_ecb_padded_size(size_t plaintext_size) {
    return ((plaintext_size / 16) + 1) * 16;
}

int aes_ecb_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                    const uint8_t *key, uint8_t *ciphertext, size_t *ciphertext_len) {
    if (!plaintext || !key || !ciphertext || !ciphertext_len) {
        return -1;
    }

    size_t padded_len = aes_ecb_padded_size(plaintext_len);
    *ciphertext_len = padded_len;

    uint8_t *padded = malloc(padded_len);
    if (!padded) return -1;

    memcpy(padded, plaintext, plaintext_len);

    size_t padding = padded_len - plaintext_len;
    memset(padded + plaintext_len, (uint8_t)padding, padding);

    AES_KEY aes_key;
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        free(padded);
        return -1;
    }

    for (size_t i = 0; i < padded_len; i += 16) {
        AES_encrypt(padded + i, ciphertext + i, &aes_key);
    }

    free(padded);
    return 0;
}
