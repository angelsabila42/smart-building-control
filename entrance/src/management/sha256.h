#ifndef MANAGEMENT_SHA256_H
#define MANAGEMENT_SHA256_H

#include <stddef.h>
#include <stdint.h>

/*
 * Small self-contained SHA-256 implementation.
 *
 * It is used only to derive administrator password hashes.  Keeping it in-tree
 * means the authentication logic can be built and unit-tested on a normal
 * development machine as well as on the ESP32.
 */

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE 64

typedef struct {
    uint32_t state[8];
    uint64_t bit_length;
    uint8_t buffer[SHA256_BLOCK_SIZE];
    size_t buffer_length;
} sha256_context_t;

void sha256_init(sha256_context_t *context);
void sha256_update(sha256_context_t *context, const void *data, size_t length);
void sha256_final(sha256_context_t *context, uint8_t digest[SHA256_DIGEST_SIZE]);

void sha256(const void *data, size_t length, uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* MANAGEMENT_SHA256_H */
