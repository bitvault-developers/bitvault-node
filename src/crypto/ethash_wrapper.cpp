#include "ethash_wrapper.h"
#include "primitives/block.h"
#include <crypto/sha256.h>
#include <vector>
#include <cstring>
#include <mutex>

extern "C" {
#include "ethash/ethash.h"
#include "ethash/sha3.h"
}

// Cache ethash_light_t per height — avoids 1M-iteration regen during mining
static std::mutex g_ethash_mutex;
static int64_t g_cached_height = -1;
static ethash_light_t g_cached_light = nullptr;

bool ComputeEthashHash(const CBlockHeader& block, int nHeight, uint256& hash)
{
    unsigned char headerHash[32];
    CSHA256 hasher;
    hasher.Write((const unsigned char*)&block.nVersion, 4);
    hasher.Write(block.hashPrevBlock.begin(), 32);
    hasher.Write(block.hashMerkleRoot.begin(), 32);
    hasher.Write((const unsigned char*)&block.nTime, 4);
    hasher.Write((const unsigned char*)&block.nBits, 4);
    hasher.Finalize(headerHash);

    ethash_h256_t seed;
    memset(&seed, 0, sizeof(seed));
    sha3_256(seed.b, 32, headerHash, 32);

    std::lock_guard<std::mutex> lock(g_ethash_mutex);
    if (g_cached_height != nHeight || !g_cached_light) {
        if (g_cached_light) {
            ethash_light_delete(g_cached_light);
            g_cached_light = nullptr;
        }
        g_cached_light = ethash_light_new(static_cast<uint64_t>(nHeight));
        g_cached_height = nHeight;
    }
    if (!g_cached_light) return false;

    ethash_return_value_t result = ethash_light_compute(g_cached_light, seed, block.nNonce);
    if (!result.success) return false;

    memcpy(hash.begin(), result.result.b, 32);
    return true;
}
