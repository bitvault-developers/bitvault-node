#include "crypto/kheavyhash.h"
#include <cstdio>

// KHeavyHash integration for BitVault mining
// 11th mining algorithm — optical-mining-friendly

void KHeavyHashCompute(const unsigned char* input, size_t len, unsigned char* output) {
    kheavyhash::compute(input, len, output);
}

bool KHeavyHashVerify(const unsigned char* input, size_t len, const unsigned char* expected) {
    return kheavyhash::verify(input, len, expected);
}

#include <uint256.h>
#include <span.h>

uint256 HashKHeavyHash(Span<const unsigned char> input) {
    unsigned char output[32];
    kheavyhash::compute(input.data(), input.size(), output);
    uint256 result;
    memcpy(result.begin(), output, 32);
    return result;
}
