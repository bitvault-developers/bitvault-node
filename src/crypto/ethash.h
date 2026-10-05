#ifndef BITVAULT_CRYPTO_ETHASH_H
#define BITVAULT_CRYPTO_ETHASH_H

#include <stdint.h>
#include <stddef.h>

/**
 * Compute the Ethash hash of a block header.
 *
 * @param header      Pointer to the block header (serialized as bytes)
 * @param header_len  Length of the header in bytes
 * @param nonce       The 64‑bit nonce
 * @param output      Buffer to receive the 32‑byte hash result
 * @return            true on success, false on error (e.g., internal failure)
 */
bool ethash_hash(const uint8_t* header, size_t header_len, uint64_t nonce, uint8_t* output);

#endif
