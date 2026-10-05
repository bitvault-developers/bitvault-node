#include <crypto/poh.h>
#include <crypto/sha256.h>
#include <cstring>

PoHProof GeneratePoH(const uint256& seed, uint32_t iterations)
{
    PoHProof proof;
    proof.iterations = iterations;

    unsigned char current[32];
    memcpy(current, seed.begin(), 32);

    for (uint32_t i = 0; i < iterations; i++) {
        unsigned char next[32];
        CSHA256().Write(current, 32).Finalize(next);
        memcpy(current, next, 32);
    }

    memcpy(proof.hash.begin(), current, 32);
    return proof;
}

bool VerifyPoH(const uint256& seed, uint32_t iterations, const uint256& claimedHash)
{
    PoHProof proof = GeneratePoH(seed, iterations);
    return proof.hash == claimedHash;
}
