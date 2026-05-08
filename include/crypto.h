#ifndef ILINK_CRYPTO_H
#define ILINK_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

int aes_ecb_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                    const uint8_t *key, uint8_t *ciphertext, size_t *ciphertext_len);
size_t aes_ecb_padded_size(size_t plaintext_size);

#endif /* ILINK_CRYPTO_H */
