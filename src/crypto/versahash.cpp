#include <crypto/versahash.h>
#include <crypto/sha256.h>
#include <span.h>
#include <uint256.h>

uint256 HashVersaHash(Span<const unsigned char> input)
{
    unsigned char round1[32], round2[32];
    CSHA256().Write(input.data(), input.size()).Finalize(round1);
    CSHA256().Write(round1, 32).Finalize(round1);
    CSHA256().Write(round1, 32).Finalize(round2);
    CSHA256().Write(round2, 32).Finalize(round2);
    unsigned char mixed[32];
    for (int i = 0; i < 32; i++)
        mixed[i] = round1[i] ^ round2[i] ^ input[i % input.size()];
    unsigned char out[32];
    CSHA256().Write(mixed, 32).Finalize(out);
    CSHA256().Write(out, 32).Finalize(out);
    uint256 result;
    memcpy(result.begin(), out, 32);
    return result;
}
