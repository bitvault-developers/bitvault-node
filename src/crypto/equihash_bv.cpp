#include <crypto/equihash_bv.h>
#include <crypto/sha256.h>
#include <cstring>

uint256 HashEquihashBV(Span<const unsigned char> input)
{
    unsigned char state[32], tmp[32];
    CSHA256().Write(input.data(), input.size()).Finalize(state);
    for (int round = 0; round < 5; round++) {
        CSHA256().Write(state, 32).Finalize(tmp);
        CSHA256().Write(tmp, 32).Finalize(state);
        for (int i = 0; i < 32; i++)
            state[i] ^= tmp[i] ^ input[i % input.size()];
    }
    unsigned char out[32];
    CSHA256().Write(state, 32).Finalize(out);
    CSHA256().Write(out, 32).Finalize(out);
    uint256 result;
    memcpy(result.begin(), out, 32);
    return result;
}
