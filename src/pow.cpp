// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <pow.h>
#include <logging.h>
#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <chainparams.h>

inline unsigned int PowLimit(const Consensus::Params& params)
{
    return UintToArith256(params.powLimit).GetCompact();
}

unsigned int InitialDifficulty(const Consensus::Params& params, int algo)
{
    const auto& it = params.initialTarget.find(algo);
    if (it == params.initialTarget.end())
        return PowLimit(params);
    return UintToArith256(it->second).GetCompact();
}

unsigned int GetNextWorkRequiredV1(const CBlockIndex* pindexLast, const Consensus::Params& params, int algo)
{
    int nHeight = pindexLast->nHeight + 1;
    bool fNewDifficultyProtocol = (nHeight >= params.nDiffChangeTarget);
    int blockstogoback = 0;

    //set default to pre-v2.0 values
    int64_t retargetTimespan = params.nTargetTimespan;
    //int64_t retargetSpacing = nTargetSpacing;
    int64_t retargetInterval = params.nInterval;

    //if v2.0 changes are in effect for block num, alter retarget values
    if(fNewDifficultyProtocol && !params.fPowAllowMinDifficultyBlocks) {
        LogPrintf("GetNextWorkRequired nActualTimespan Limiting\n");
        retargetTimespan = params.nTargetTimespanRe;
        //retargetSpacing = nTargetSpacingRe;
        retargetInterval = params.nIntervalRe;
    }

    // Only change once per interval
    if ((pindexLast->nHeight+1) % retargetInterval != 0)
    {
        return pindexLast->nBits;
    }

    // BitVault: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    blockstogoback = retargetInterval-1;
    if ((pindexLast->nHeight+1) != retargetInterval)
        blockstogoback = retargetInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();

    // thanks to RealSolid & WDC for this code
    if(fNewDifficultyProtocol && !params.fPowAllowMinDifficultyBlocks) {
        if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/4));
        if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/2));
    }
    else {
        if (nActualTimespan < retargetTimespan/4) nActualTimespan = retargetTimespan/4;
        if (nActualTimespan > retargetTimespan*4) nActualTimespan = retargetTimespan*4;
    }

    arith_uint256 bnNew;
    arith_uint256 bnBefore;
    bnNew.SetCompact(pindexLast->nBits);
    bnBefore=bnNew;
    bnNew *= nActualTimespan;
    bnNew /= retargetTimespan;

    if (bnNew > UintToArith256(params.powLimit))
        bnNew = UintToArith256(params.powLimit);

    // debug print
    LogPrintf("nTargetTimespan = %d    nActualTimespan = %d\n", retargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, ArithToUint256(bnBefore).ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString());

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequiredV2(const CBlockIndex* pindexLast, const Consensus::Params& params, int algo)
{
    LogPrintf("Height (Before): %s\n", pindexLast->nHeight);

    // find previous block with same algo
    const CBlockIndex* pindexPrev = GetLastBlockIndexForAlgo(pindexLast, params, algo);

    // find first block in averaging interval
    // Go back by what we want to be nAveragingInterval blocks
    const CBlockIndex* pindexFirst = pindexPrev;
    for (int i = 0; pindexFirst && i < params.nAveragingInterval - 1; i++)
    {
        pindexFirst = pindexFirst->pprev;
        pindexFirst = GetLastBlockIndexForAlgo(pindexFirst, params, algo);
    }

    if (pindexFirst == nullptr)
    {
        LogPrintf("Use default POW Limit\n");
        return InitialDifficulty(params, algo);
    }

    // Limit adjustment step
    int64_t nActualTimespan = pindexPrev->GetBlockTime() - pindexFirst->GetBlockTime();
    if (nActualTimespan < params.nMinActualTimespan)
        nActualTimespan = params.nMinActualTimespan;
    if (nActualTimespan > params.nMaxActualTimespan)
        nActualTimespan = params.nMaxActualTimespan;

    // Retarget

    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nAveragingTargetTimespan;

    if (bnNew > UintToArith256(params.powLimit))
    {
        bnNew = UintToArith256(params.powLimit);
    }

    return bnNew.GetCompact();    
}

unsigned int GetNextWorkRequiredV3(const CBlockIndex* pindexLast, const Consensus::Params& params, int algo)
{
    // find first block in averaging interval
    // Go back by what we want to be nAveragingInterval blocks per algo
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < NUM_ALGOS*params.nAveragingInterval; i++)
    {
        pindexFirst = pindexFirst->pprev;
    }
    const CBlockIndex* pindexPrevAlgo = GetLastBlockIndexForAlgo(pindexLast, params, algo);
    if (pindexPrevAlgo == nullptr || pindexFirst == nullptr)
        return InitialDifficulty(params, algo); // not enough blocks available

    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = pindexLast->GetMedianTimePast() - pindexFirst->GetMedianTimePast();
    nActualTimespan = params.nAveragingTargetTimespan + (nActualTimespan - params.nAveragingTargetTimespan)/6;
    if (nActualTimespan < params.nMinActualTimespanV3)
        nActualTimespan = params.nMinActualTimespanV3;
    if (nActualTimespan > params.nMaxActualTimespanV3)
        nActualTimespan = params.nMaxActualTimespanV3;

    // Global retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrevAlgo->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nAveragingTargetTimespan;

    // Per-algo retarget
    int nAdjustments = pindexPrevAlgo->nHeight - pindexLast->nHeight + NUM_ALGOS - 1;
    if (nAdjustments > 0)
    {
        for (int i = 0; i < nAdjustments; i++)
        {
            bnNew *= 100;
            bnNew /= 100 + params.nLocalDifficultyAdjustment;
        }
    }
    if (nAdjustments < 0)
    {
        for (int i = 0; i < -nAdjustments; i++)
        {
            bnNew *= 100 + params.nLocalDifficultyAdjustment;
            bnNew /= 100;
        }
    }

    if (bnNew > UintToArith256(params.powLimit))
        bnNew = UintToArith256(params.powLimit);

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequiredV4(const CBlockIndex* pindexLast, const Consensus::Params& params, int algo)
{
    // find first block in averaging interval
    // Go back by what we want to be nAveragingInterval blocks per algo
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < NUM_ALGOS*params.nAveragingInterval; i++)
    {
        pindexFirst = pindexFirst->pprev;
    }

    const CBlockIndex* pindexPrevAlgo = GetLastBlockIndexForAlgoFast(pindexLast, params, algo);
    if (pindexPrevAlgo == nullptr || pindexFirst == nullptr)
    {
        return InitialDifficulty(params, algo);
    }

    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = pindexLast-> GetMedianTimePast() - pindexFirst->GetMedianTimePast();
    nActualTimespan = params.nAveragingTargetTimespanV4 + (nActualTimespan - params.nAveragingTargetTimespanV4)/4;

    if (nActualTimespan < params.nMinActualTimespanV4)
        nActualTimespan = params.nMinActualTimespanV4;
    if (nActualTimespan > params.nMaxActualTimespanV4)
        nActualTimespan = params.nMaxActualTimespanV4;

    //Global retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrevAlgo->nBits);

    bnNew *= nActualTimespan;
    bnNew /= params.nAveragingTargetTimespanV4;

    //Per-algo retarget
    int nAdjustments = pindexPrevAlgo->nHeight + NUM_ALGOS - 1 - pindexLast->nHeight;
    if (nAdjustments > 0)
    {
        for (int i = 0; i < nAdjustments; i++)
        {
            bnNew *= 100;
            bnNew /= (100 + params.nLocalTargetAdjustment);
        }
    }
    else if (nAdjustments < 0)//make it easier
    {
        for (int i = 0; i < -nAdjustments; i++)
        {
            bnNew *= (100 + params.nLocalTargetAdjustment);
            bnNew /= 100;
            if (bnNew > UintToArith256(params.powLimit)) {
              bnNew = UintToArith256(params.powLimit);
              break;
            }            
        }
    }

    if (bnNew > UintToArith256(params.powLimit))
    {
        bnNew = UintToArith256(params.powLimit);
    }

    return bnNew.GetCompact();
}


// ── BVT_ASERT_FORK: deterministic per-algo difficulty from height 12374 ──────────
// The legacy path walked pindex->lastAlgoBlocks[], an in-memory table that is
// not reliably populated for every block, so the anchor lookup could fail and
// silently fall back to powLimit. This walks pprev directly: same answer every
// time, on every node, after any restart or reindex.
static const int BVT_ASERT_FORK_HEIGHT = 12374;

static const CBlockIndex* BVTFindPrevBlockForAlgo(const CBlockIndex* pindex, int algo, int maxDepth)
{
    for (int i = 0; pindex && i < maxDepth; ++i, pindex = pindex->pprev) {
        if (pindex->GetAlgo() == algo) return pindex;
    }
    return nullptr;
}

static unsigned int BVTNextWorkASERT(const CBlockIndex* pindexLast, const Consensus::Params& params, int algo)
{
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    if (!pindexLast) return powLimit.GetCompact();

    const CBlockIndex* anchor = BVTFindPrevBlockForAlgo(pindexLast, algo, 100000);
    if (!anchor || !anchor->pprev) return powLimit.GetCompact();
    const CBlockIndex* ref = BVTFindPrevBlockForAlgo(anchor->pprev, algo, 100000);
    if (!ref) return powLimit.GetCompact();

    arith_uint256 anchorTarget;
    anchorTarget.SetCompact(anchor->nBits);
    if (anchorTarget == 0 || anchorTarget > powLimit) anchorTarget = powLimit;

    int64_t timeDiff   = (int64_t)anchor->nTime - (int64_t)ref->nTime;
    int64_t heightDiff = (int64_t)anchor->nHeight - (int64_t)ref->nHeight;
    if (heightDiff <= 0) return anchorTarget.GetCompact();

    const int64_t kAlgoSpacing = 660;   // 11 algos x 60s, matches ALGO_TARGET_SPACING
    const int64_t kHalfLife     = 3600;  // matches ASERT_HALFLIFE
    const int64_t idealTime = kAlgoSpacing * heightDiff;
    int64_t exponent = ((timeDiff - idealTime) * 65536) / kHalfLife;

    // clamp so a single wild timestamp cannot swing difficulty violently
    const int64_t kMaxExp = 4 * 65536;
    if (exponent >  kMaxExp) exponent =  kMaxExp;
    if (exponent < -kMaxExp) exponent = -kMaxExp;

    arith_uint256 next = anchorTarget;
    if (exponent >= 0) {
        int64_t shifts = exponent >> 16, frac = exponent & 0xFFFF;
        next = next + ((next >> 1) * (uint32_t)frac >> 16);
        while (shifts-- > 0 && next < powLimit) next <<= 1;
    } else {
        int64_t neg = -exponent, shifts = neg >> 16, frac = neg & 0xFFFF;
        next = next - ((next >> 1) * (uint32_t)frac >> 16);
        while (shifts-- > 0 && next > 1) next >>= 1;
    }

    if (next == 0) next = 1;
    if (next > powLimit) next = powLimit;
    return next.GetCompact();
}


// ── BVT_ASERT_FORK2 (height 12600): the per-algo search compares a caller algo id
// against CBlockIndex::GetAlgo(); on this chain they never match, so the lookup
// found nothing and difficulty sat at powLimit forever. When the per-algo search
// fails, anchor on the chain tip. For a single-algorithm chain that is the correct
// behaviour, targeting the chain spacing (60s) rather than the per-algo 660s.
static const int BVT_ASERT_FORK2_HEIGHT = 12600;

static unsigned int BVTNextWorkASERT2(const CBlockIndex* pindexLast, const Consensus::Params& params, int algo)
{
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    if (!pindexLast || !pindexLast->pprev) return powLimit.GetCompact();

    int64_t spacing = 660;
    const CBlockIndex* anchor = BVTFindPrevBlockForAlgo(pindexLast, algo, 100000);
    const CBlockIndex* ref = nullptr;
    if (anchor && anchor->pprev) ref = BVTFindPrevBlockForAlgo(anchor->pprev, algo, 100000);
    if (!anchor || !ref) { anchor = pindexLast; ref = pindexLast->pprev; spacing = 60; }

    arith_uint256 anchorTarget;
    anchorTarget.SetCompact(anchor->nBits);
    if (anchorTarget == 0 || anchorTarget > powLimit) anchorTarget = powLimit;

    int64_t timeDiff   = (int64_t)anchor->nTime - (int64_t)ref->nTime;
    int64_t heightDiff = (int64_t)anchor->nHeight - (int64_t)ref->nHeight;
    if (heightDiff <= 0) heightDiff = 1;
    if (timeDiff < 1) timeDiff = 1;

    const int64_t halfLife = 3600;
    int64_t exponent = ((timeDiff - spacing * heightDiff) * 65536) / halfLife;
    const int64_t kMaxExp = 4 * 65536;
    if (exponent >  kMaxExp) exponent =  kMaxExp;
    if (exponent < -kMaxExp) exponent = -kMaxExp;

    arith_uint256 next = anchorTarget;
    if (exponent >= 0) {
        int64_t shifts = exponent >> 16, frac = exponent & 0xFFFF;
        next = next + ((next >> 1) * arith_uint256((uint64_t)frac) >> 16);
        while (shifts-- > 0 && next < powLimit) next <<= 1;
    } else {
        int64_t neg = -exponent, shifts = neg >> 16, frac = neg & 0xFFFF;
        next = next - ((next >> 1) * arith_uint256((uint64_t)frac) >> 16);
        while (shifts-- > 0 && next > 1) next >>= 1;
    }
    if (next == 0) next = 1;
    if (next > powLimit) next = powLimit;
    LogPrintf("ASERT2 h=%d algo=%d tipAlgo=%d mode=%s tdiff=%d out=%08x\n",
              pindexLast->nHeight + 1, algo, pindexLast->GetAlgo(), spacing == 60 ? "chain" : "algo",
              (int)timeDiff, next.GetCompact());
    return next.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, int algo)
{
    // Genesis block
    if (pindexLast == nullptr)
        return InitialDifficulty(params, algo);

    if (params.fPowAllowMinDifficultyBlocks)
    {
        // Special difficulty rule for regtest:
        // Always allow min difficulty blocks if fEasyPow is set
        if (params.fEasyPow) {
            return PowLimit(params);
        }

        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 2 minutes
        // then allow mining of a min-difficulty block.
        if (pblock->nTime > pindexLast->nTime + params.nTargetSpacing*2)
            return PowLimit(params);
    }

    if (pindexLast->nHeight < 100)
        return GetNextWorkRequiredV1(pindexLast, params, algo);
    // V5: ASERT activates at height 200+ for responsive per-algo difficulty
    if (pindexLast->nHeight >= 200)
    {
        if (pindexLast->nHeight + 1 >= BVT_ASERT_FORK_HEIGHT)
            if (pindexLast->nHeight + 1 >= BVT_ASERT_FORK2_HEIGHT)
                return BVTNextWorkASERT2(pindexLast, params, algo);
            return BVTNextWorkASERT(pindexLast, params, algo);
        return GetNextWorkRequiredV5_ASERT(pindexLast, params, algo);
    }
    if (pindexLast->nHeight < params.multiAlgoDiffChangeTarget)
        return GetNextWorkRequiredV1(pindexLast, params, algo);
    else if (pindexLast->nHeight < params.alwaysUpdateDiffChangeTarget){
        return GetNextWorkRequiredV2(pindexLast, params, algo);
    } else if(pindexLast->nHeight < params.workComputationChangeTarget)
        return GetNextWorkRequiredV3(pindexLast, params, algo);
    else
        return GetNextWorkRequiredV4(pindexLast, params, algo);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// Check that on difficulty adjustments, the new difficulty does not increase
// or decrease beyond the permitted limits.
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    if (params.fPowAllowMinDifficultyBlocks) return true;

    // BitVault v8.22.2 worked perfectly without this BitVault Core v26.2 difficulty validation.
    // BitVault uses real-time MultiShield difficulty adjustment on every single block across
    // all 4 difficulty eras (V1, V2, V3, V4), which is fundamentally incompatible with 
    // BitVault's 2016-block difficulty validation model.
    // Disable this validation entirely for ALL BitVault networks and rely on the proper
    // BitVault difficulty validation that occurs in the block validation pipeline.
    if (params.nPowTargetSpacing == 15) {
        // This is BitVault (ALL networks use 15-second blocks)
        // Completely bypass BitVault's validation since BitVault adjusts
        // difficulty in real-time on every block in all 4 eras
        return true;
    }

    if (height % params.DifficultyAdjustmentInterval() == 0) {
        int64_t smallest_timespan = params.nPowTargetTimespan/4;
        int64_t largest_timespan = params.nPowTargetTimespan*4;

        const arith_uint256 pow_limit = UintToArith256(params.powLimit);
        arith_uint256 observed_new_target;
        observed_new_target.SetCompact(new_nbits);

        // Calculate the largest difficulty value possible:
        arith_uint256 largest_difficulty_target;
        largest_difficulty_target.SetCompact(old_nbits);
        largest_difficulty_target *= largest_timespan;
        largest_difficulty_target /= params.nPowTargetTimespan;

        if (largest_difficulty_target > pow_limit) {
            largest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 maximum_new_target;
        maximum_new_target.SetCompact(largest_difficulty_target.GetCompact());
        if (maximum_new_target < observed_new_target) return false;

        // Calculate the smallest difficulty value possible:
        arith_uint256 smallest_difficulty_target;
        smallest_difficulty_target.SetCompact(old_nbits);
        smallest_difficulty_target *= smallest_timespan;
        smallest_difficulty_target /= params.nPowTargetTimespan;

        if (smallest_difficulty_target > pow_limit) {
            smallest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 minimum_new_target;
        minimum_new_target.SetCompact(smallest_difficulty_target.GetCompact());
        if (minimum_new_target > observed_new_target) return false;
    } else if (old_nbits != new_nbits) {
        return false;
    }
    return true;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit)) {
        LogPrint(BCLog::VALIDATION, "PoWDBG range-fail neg=%d zero=%d ovf=%d bits=%08x target=%s powLimit=%s\n",
                  (int)fNegative, (int)(bnTarget == 0), (int)fOverflow, nBits,
                  bnTarget.GetHex(), UintToArith256(params.powLimit).GetHex());
        return false;
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget) {
        LogPrint(BCLog::VALIDATION, "PoWDBG hash-fail bits=%08x hash=%s target=%s\n",
                  nBits, hash.ToString(), bnTarget.GetHex());
        return false;
    }

    return true;
}

const CBlockIndex* GetLastBlockIndexForAlgo(const CBlockIndex* pindex, const Consensus::Params& params, int algo)
{
    for (; pindex; pindex = pindex->pprev)
    {
        if (pindex->GetAlgo() != algo)
            continue;
        // ignore special min-difficulty testnet blocks
        if (params.fPowAllowMinDifficultyBlocks &&
            pindex->pprev &&
            pindex->nTime > pindex->pprev->nTime + params.nTargetSpacing*2)
        {
            continue;
        }
        return pindex;
    }
    return nullptr;
}

const CBlockIndex* GetLastBlockIndexForAlgoFast(const CBlockIndex* pindex, const Consensus::Params& params, int algo)
{
    for (; pindex; pindex = pindex->lastAlgoBlocks[algo])
    {
        if (pindex->GetAlgo() != algo)
            continue;
        if (params.fPowAllowMinDifficultyBlocks &&
            pindex->pprev &&
            pindex->nTime > pindex->pprev->nTime + params.nTargetSpacing*2)
        {
            pindex = pindex->pprev;
            continue;
        }
        return pindex;
    }

    return nullptr;
}

uint256 GetPoWAlgoHash(const CBlockHeader& block)
{
    return block.GetPoWAlgoHash(Params().GetConsensus());
}

// ─── ASERT V5: Absolute Ease-of-Service Re-Target (per-algo) ─────────────
// 60s block time * 11 algos = 660s ideal spacing between same-algo blocks
static const int64_t ASERT_HALFLIFE     = 3600;   // 1 hour
static const int64_t ALGO_TARGET_SPACING = 660;   // 11 algos * 60s

unsigned int GetNextWorkRequiredV5_ASERT(const CBlockIndex* pindexLast,
    const Consensus::Params& params, int algo)
{
    const CBlockIndex* pindexAnchor = GetLastBlockIndexForAlgoFast(pindexLast, params, algo);
    if (!pindexAnchor || !pindexAnchor->pprev)
        return UintToArith256(params.powLimit).GetCompact();

    const CBlockIndex* pindexRef = GetLastBlockIndexForAlgoFast(pindexAnchor->pprev, params, algo);
    if (!pindexRef)
        return UintToArith256(params.powLimit).GetCompact();

    arith_uint256 anchorTarget;
    anchorTarget.SetCompact(pindexAnchor->nBits);

    int64_t timeDiff   = (int64_t)pindexAnchor->nTime - (int64_t)pindexRef->nTime;
    int64_t heightDiff = pindexAnchor->nHeight - pindexRef->nHeight;
    if (heightDiff <= 0) heightDiff = 1;

    int64_t idealTime = ALGO_TARGET_SPACING * heightDiff;
    int64_t exponent  = ((timeDiff - idealTime) * 65536) / ASERT_HALFLIFE;

    arith_uint256 nextTarget = anchorTarget;
    if (exponent >= 0) {
        int64_t shifts = exponent >> 16;
        uint64_t frac  = (uint64_t)(exponent & 0xFFFF);
        nextTarget = nextTarget + ((nextTarget >> 1) * frac >> 16);
        if (shifts > 0) nextTarget <<= (int)std::min(shifts, (int64_t)255);
    } else {
        int64_t negExp = -exponent;
        int64_t shifts = negExp >> 16;
        uint64_t frac  = (uint64_t)(negExp & 0xFFFF);
        nextTarget = nextTarget - ((nextTarget >> 1) * frac >> 16);
        if (shifts > 0) nextTarget >>= (int)std::min(shifts, (int64_t)255);
    }

    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    if (nextTarget > powLimit || nextTarget == arith_uint256())
        nextTarget = powLimit;

    return nextTarget.GetCompact();
}
