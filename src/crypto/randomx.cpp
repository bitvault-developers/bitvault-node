#include "randomx.h"
#include <cstring>

extern "C" {
#include <randomx.h>
}

static randomx_flags g_flags = RANDOMX_FLAG_DEFAULT;
static randomx_cache* g_cache = nullptr;
static randomx_vm* g_vm = nullptr;
static bool g_init_failed = false;

static void EnsureInitialized()
{
    if (g_vm) return;
    if (g_init_failed) return;

    g_flags = randomx_get_flags();
    // Mask JIT — W^X memory restrictions in many environments cause SIGSEGV
    g_flags = (randomx_flags)(g_flags & ~RANDOMX_FLAG_JIT);

    g_cache = randomx_alloc_cache(g_flags);
    if (!g_cache) { g_init_failed = true; return; }

    const char* key = "BitVaultRandomXKey";
    randomx_init_cache(g_cache, key, strlen(key));

    g_vm = randomx_create_vm(g_flags, g_cache, nullptr);
    if (!g_vm) {
        randomx_release_cache(g_cache);
        g_cache = nullptr;
        g_init_failed = true;
        return;
    }
}

uint256 RandomXHash(const std::vector<unsigned char>& input)
{
    EnsureInitialized();
    uint256 result;
    if (!g_vm) {
        // init failed — return max-value hash (cleanly fails any target)
        memset(result.begin(), 0xFF, 32);
        return result;
    }
    unsigned char hash[32];
    randomx_calculate_hash(g_vm, input.data(), input.size(), hash);
    memcpy(result.begin(), hash, 32);
    return result;
}
