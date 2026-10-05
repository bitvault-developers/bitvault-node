#ifndef BITVAULT_FEATURES_H
#define BITVAULT_FEATURES_H

// ============================================================
// BitVault L1 Advanced Consensus Features
// May 2026 — DGB/BTC fork enhanced
// ============================================================

#include <primitives/block.h>
#include <cstdint>

// ── 1. MINER DIVERSITY BONUS ─────────────────────────────────
// Rewards miners using underrepresented algorithms.
// Applied in GetBlockSubsidy: +10% for least-used, +5% near-least.
// Encourages all 11 algorithms to have active miners.

inline int GetDiversityBonusPercent(int algo, const int algoCounts[]) {
    int total = 0, minCount = 999999;
    for (int i = 0; i < NUM_ALGOS; i++) {
        total += algoCounts[i];
        if (algoCounts[i] < minCount) minCount = algoCounts[i];
    }
    if (total == 0) return 0;
    // Extra bonus tiers
    if (algoCounts[algo] == minCount)      return 10; // least used → 10% bonus
    if (algoCounts[algo] <= minCount + 1)  return 5;  // near-least  → 5%  bonus
    return 0;
}

// ── 2. ADAPTIVE ALGORITHM ROTATION ("AI Rotation") ──────────
// Data-driven algo selection — routes new blocks to the most
// underrepresented algorithm based on recent block history.
// Falls back to epoch rotation when chain is new (<2 blocks/algo).

inline int GetAdaptiveAlgo(int nHeight, const int algoCounts[]) {
    int total = 0;
    for (int i = 0; i < NUM_ALGOS; i++) total += algoCounts[i];

    // Bootstrap: use round-robin until enough history
    if (total < NUM_ALGOS * 2) return nHeight % NUM_ALGOS;

    // Find algo furthest below expected equal distribution
    int expected = total / NUM_ALGOS;
    int bestAlgo = nHeight % NUM_ALGOS; // fallback
    int maxDeficit = -1;
    for (int i = 0; i < NUM_ALGOS; i++) {
        int deficit = expected - algoCounts[i];
        if (deficit > maxDeficit) {
            maxDeficit = deficit;
            bestAlgo = i;
        }
    }
    return bestAlgo;
}

// Legacy epoch-based rotation (kept for compatibility)
inline int GetPreferredAlgo(int nHeight) {
    return (nHeight / 1000) % NUM_ALGOS;
}

inline bool IsPreferredAlgoForEpoch(int nHeight, int algo) {
    int preferred = GetPreferredAlgo(nHeight);
    return (algo == preferred ||
            algo == ((preferred + 1) % NUM_ALGOS) ||
            algo == ((preferred + 2) % NUM_ALGOS));
}

// Difficulty weight for algo (used in getblocktemplate hints)
inline int GetAlgoDifficultyWeight(int algo, const int algoCounts[]) {
    int total = 0;
    for (int i = 0; i < NUM_ALGOS; i++) total += algoCounts[i];
    if (total == 0) return 100;
    int expected = total / NUM_ALGOS;
    if (expected == 0) return 100;
    int w = (algoCounts[algo] * 100) / expected;
    if (w < 50)  w = 50;
    if (w > 200) w = 200;
    return w;
}

// ── 3. PoS VALIDATORS ────────────────────────────────────────
// Lightweight PoS finality layer.
// 5 validator slots earn rewards from each block.
// Validators selected round-robin by block height.
// Future: validators must stake minimum BVT to remain in set.
//
// Reward: 2.5% of block reward (75 BVT at 3000 base)
// Funded from APPS_TREASURY portion (apps receives 57.5% instead of 60%)

static const int NUM_POS_VALIDATORS = 5;

// Returns validator index for this block height (0..NUM_POS_VALIDATORS-1)
inline int GetPoSValidatorIndex(int nHeight) {
    return nHeight % NUM_POS_VALIDATORS;
}

// ── 4. DPoS DELEGATES ────────────────────────────────────────
// 11 delegate slots — one per mining algorithm.
// When algorithm X mines a block, delegate X earns a reward.
// Creates alignment between delegates and their algorithm miners.
// Future: BVT holders vote to change delegate addresses.
//
// Reward: 2.5% of block reward (75 BVT at 3000 base)
// Funded from APPS_TREASURY portion

static const int NUM_DPOS_DELEGATES = NUM_ALGOS; // 11

// Returns DPoS delegate index for the algorithm that mined this block
inline int GetDPoSDelegateIndex(int nAlgo) {
    if (nAlgo < 0 || nAlgo >= NUM_ALGOS) return 0;
    return nAlgo; // delegate slot = algo index
}

#endif // BITVAULT_FEATURES_H
