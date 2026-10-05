#ifndef BITVAULT_CRYPTO_POH_H
#define BITVAULT_CRYPTO_POH_H

#include <uint256.h>
#include <cstdint>

struct PoHProof {
    uint256 hash;
    uint32_t iterations;
};

/** Generate a PoH proof: iterate SHA256 from seed for n iterations */
PoHProof GeneratePoH(const uint256& seed, uint32_t iterations);

/** Verify a PoH proof: re-run the chain and check final hash matches */
bool VerifyPoH(const uint256& seed, uint32_t iterations, const uint256& claimedHash);

#endif
