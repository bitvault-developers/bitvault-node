#ifndef BITVAULT_CRYPTO_ETHASH_WRAPPER_H
#define BITVAULT_CRYPTO_ETHASH_WRAPPER_H

#include <uint256.h>

class CBlockHeader;

/** Compute Ethash hash for a block header. Returns true on success. */
bool ComputeEthashHash(const CBlockHeader& block, int nHeight, uint256& hash);

#endif
