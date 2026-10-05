// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <node/miner.h>
#include <bitvault_activation.h>
#include <key_io.h>
#include <crypto/poh.h>

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <common/args.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <deploymentstatus.h>
#include <logging.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <key_io.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <timedata.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <validation.h>
#include <pubkey.h>
#include <key.h>
#include <algorithm>
#include <utility>

namespace node {

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev, int algo)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime{std::max<int64_t>(pindexPrev->GetMedianTimePast() + 1, TicksSinceEpoch<std::chrono::seconds>(GetAdjustedTime()))};

    if (nOldTime < nNewTime) {
        pblock->nTime = nNewTime;
    }

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams, algo);
    }

    return nNewTime - nOldTime;
}

void RegenerateCommitments(CBlock& block, ChainstateManager& chainman)
{
    CMutableTransaction tx{*block.vtx.at(0)};
    tx.vout.erase(tx.vout.begin() + GetWitnessCommitmentIndex(block));
    block.vtx.at(0) = MakeTransactionRef(tx);

    const CBlockIndex* prev_block = WITH_LOCK(::cs_main, return chainman.m_blockman.LookupBlockIndex(block.hashPrevBlock));
    chainman.GenerateCoinbaseCommitment(block, prev_block);

    block.hashMerkleRoot = BlockMerkleRoot(block);
}

static BlockAssembler::Options ClampOptions(BlockAssembler::Options options)
{
    // Limit weight to between 4K and DEFAULT_BLOCK_MAX_WEIGHT for sanity:
    options.nBlockMaxWeight = std::clamp<size_t>(options.nBlockMaxWeight, 4000, DEFAULT_BLOCK_MAX_WEIGHT);
    return options;
}

BlockAssembler::BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool, const Options& options)
    : chainparams{chainstate.m_chainman.GetParams()},
      m_mempool{mempool},
      m_chainstate{chainstate},
      m_options{ClampOptions(options)}
{
}

void ApplyArgsManOptions(const ArgsManager& args, BlockAssembler::Options& options)
{
    // Block resource limits
    options.nBlockMaxWeight = args.GetIntArg("-blockmaxweight", options.nBlockMaxWeight);
    if (const auto blockmintxfee{args.GetArg("-blockmintxfee")}) {
        if (const auto parsed{ParseMoney(*blockmintxfee)}) options.blockMinFeeRate = CFeeRate{*parsed};
    }
}
static BlockAssembler::Options ConfiguredOptions()
{
    BlockAssembler::Options options;
    ApplyArgsManOptions(gArgs, options);
    return options;
}

BlockAssembler::BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool)
    : BlockAssembler(chainstate, mempool, ConfiguredOptions()) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, int algo)
{
    const auto time_start{SteadyClock::now()};

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if (!pblocktemplate.get()) {
        return nullptr;
    }
    CBlock* const pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK(::cs_main);
    CBlockIndex* pindexPrev = m_chainstate.m_chain.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    // BitVault: Check if algorithm is active before creating block
    if (!IsAlgoActive(pindexPrev, chainparams.GetConsensus(), algo))
        throw std::runtime_error(strprintf("Algorithm '%s' is not currently active.", GetAlgoName(algo).c_str()));

    pblock->nVersion = m_chainstate.m_chainman.m_versionbitscache.ComputeBlockVersion(pindexPrev, chainparams.GetConsensus(), algo);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand()) {
        pblock->nVersion = gArgs.GetIntArg("-blockversion", pblock->nVersion);
    }

    pblock->nTime = TicksSinceEpoch<std::chrono::seconds>(GetAdjustedTime());
    m_lock_time_cutoff = pindexPrev->GetMedianTimePast();

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    if (m_mempool) {
        LOCK(m_mempool->cs);
        addPackageTxs(*m_mempool, nPackagesSelected, nDescendantsUpdated);
    }

    const auto time_1{SteadyClock::now()};

    m_last_block_num_txs = nBlockTx;
    m_last_block_weight = nBlockWeight;

    // Create coinbase transaction - full reward to miner address
    // ── Count recent per-algo blocks for diversity bonus + adaptive rotation ──
    int algoCounts[NUM_ALGOS] = {};
    {
        const CBlockIndex* pIdx = pindexPrev;
        for (int i = 0; i < 100 && pIdx; i++, pIdx = pIdx->pprev) {
            int a = pIdx->GetAlgo();
            if (a >= 0 && a < NUM_ALGOS) algoCounts[a]++;
        }
    }

    // GetBlockSubsidy applies 3000 BVT base + miner diversity bonus (5-10% for rare algos)
    CAmount nSubsidy = GetBlockSubsidy(nHeight, chainparams.GetConsensus(), algo, algoCounts);
    CAmount nTotalReward = nSubsidy + nFees;

    // === BitVault Token Economics (FINAL May 2026) ===
    // Base: 3000 BVT/block (+ up to 10% diversity bonus for miner)
    // Split: 30% miner / 55% APPS / 2.5% PoS validator / 2.5% DPoS delegate / 10% team
    //
    //   Feature 1: Miner Diversity Bonus — +5-10% for rare algo miners (in GetBlockSubsidy)
    //   Feature 2: Adaptive Rotation    — AI-selects underrepresented algo (in GetAdaptiveAlgo)
    //   Feature 3: PoS Validators       — 5 validator slots, round-robin by height
    //   Feature 4: DPoS Delegates       — 11 algo-linked delegates, earn when their algo mines
    //
    //   30%   miner         =  900 BVT (+diversity bonus)  → scriptPubKeyIn
    //   55%   APPS          = 1650 BVT                     → apps treasury
    //   2.5%  PoS validator =   75 BVT                     → selected by height % 5
    //   2.5%  DPoS delegate =   75 BVT                     → delegate for this algo
    //    5%   OWNER_ARYAN   =  150 BVT                     → founder
    //    1%   SAAVARIYA     =   30 BVT
    //    1%   GOVIND        =   30 BVT
    //    1%   BAJRANG       =   30 BVT
    //    1%   KHATU         =   30 BVT
    //    1%   NARSING       =   30 BVT  (+dust)

    CAmount nMinerReward     = (nTotalReward * 10)  / 100;  // 10%
    CAmount nAppsReward      = (nTotalReward * 50)  / 100;  // 50%
    CAmount nOwnerAReward    = (nTotalReward * 5)   / 100;  //  5% OWNER_A
    CAmount nOwnerBReward    = (nTotalReward * 5)   / 100;  //  5% OWNER_B
    CAmount nOwnerCReward    = (nTotalReward * 5)   / 100;  //  5% OWNER_C
    CAmount nOwnerDReward    = (nTotalReward * 5)   / 100;  //  5% OWNER_D
    CAmount nOwnerEReward    = (nTotalReward * 5)   / 100;  //  5% OWNER_E
    CAmount nCharity1Reward  = (nTotalReward * 1)   / 100;  //  1% CHARITY_1
    CAmount nCharity2Reward  = (nTotalReward * 1)   / 100;  //  1% CHARITY_2
    CAmount nCharity3Reward  = (nTotalReward * 1)   / 100;  //  1% CHARITY_3
    CAmount nCharity4Reward  = (nTotalReward * 1)   / 100;  //  1% CHARITY_4
    CAmount nCharity5Reward  = (nTotalReward * 1)   / 100;  //  1% CHARITY_5
    CAmount nSaavariyaReward = (nTotalReward * 1)   / 100;  //  1% SAAVARIYA
    CAmount nGovindReward    = (nTotalReward * 1)   / 100;  //  1% GOVIND
    CAmount nBajrangReward   = (nTotalReward * 1)   / 100;  //  1% BAJRANG
    CAmount nKhatuReward     = (nTotalReward * 1)   / 100;  //  1% KHATU
    CAmount nNarsingReward   = (nTotalReward * 1)   / 100;  //  1% NARSING
    CAmount nShivReward      = (nTotalReward * 1)   / 100;  //  1% SHIV
    CAmount nShaniReward     = (nTotalReward * 1)   / 100;  //  1% SHANI
    CAmount nBharavReward    = (nTotalReward * 1)   / 100;  //  1% BHARAV
    CAmount nBraspathReward  = (nTotalReward * 1)   / 100;  //  1% BRASPATH
    CAmount nGaneshReward    = nTotalReward - nMinerReward - nAppsReward
                             - nOwnerAReward - nOwnerBReward - nOwnerCReward
                             - nOwnerDReward - nOwnerEReward
                             - nCharity1Reward - nCharity2Reward - nCharity3Reward
                             - nCharity4Reward - nCharity5Reward
                             - nSaavariyaReward - nGovindReward - nBajrangReward
                             - nKhatuReward - nNarsingReward - nShivReward
                             - nShaniReward - nBharavReward - nBraspathReward; // 1%+dust

    // Coinbase output scripts
    auto makeScript = [](const char* addr) {
        return GetScriptForDestination(DecodeDestination(std::string(addr)));
    };
    CScript appsScript     = makeScript("bv1qelcc7mpefc55gf68zu5v2v7e9chsyse8p0h6tn");
    CScript ownerAScript   = makeScript("bv1qgm5q6fgdtu5skgzhgc970p7zz94tx9st08hv2v");
    CScript ownerBScript   = makeScript("bv1q72m29d5ys0507e2ald3alhga0fy608zpq9gux8");
    CScript ownerCScript   = makeScript("bv1qhzmdv4gnl7xtmv8wfjg3656lmx3qh7zy39hq43");
    CScript ownerDScript   = makeScript("bv1q88mlt8csplws699runrdeghg74hhdj9x7e0ntw");
    CScript ownerEScript   = makeScript("bv1qzp8rn9fdvtwp7hqq9e5h9clwzu5u08x3yxp4fd");
    CScript charity1Script = makeScript("bv1qcn4vz34spth6v6lvclu2my5ajqqjrewawfa58w");
    CScript charity2Script = makeScript("bv1qh4enn5kpyer5njhqalpgxq3qyxzqax7zs083df");
    CScript charity3Script = makeScript("bv1q4k74nzlxak5vj2uhdjtquwsj6q75gn0249qsfh");
    CScript charity4Script = makeScript("bv1qpas4swr0plx7nz5qm90wr39kq87djfjaka2h9u");
    CScript charity5Script = makeScript("bv1qdtq0uvt44ha5rlzrvrj36ygdq48pexkuaw50py");
    CScript saavariyaScript= makeScript("bv1qptg9ppf5znsl4sa6rwwztfynheya8ahl2k0m0h");
    CScript govindScript   = makeScript("bv1qfma8yznu6hzvjkctjlcz5v86cmsvyhmvwf4y3u");
    CScript bajrangScript  = makeScript("bv1q5t6la7ysrhftrzz0dsa6pwf6lef37nmh5aef3m");
    CScript khatuScript    = makeScript("bv1qza3r44qy2dx7v2ek4tprtnf35m3v3rwfaflfct");
    CScript narsingScript  = makeScript("bv1qqhc06jsgxs9970kxe4k8eymgjaurxd5ch6mzvv");
    CScript shivScript     = makeScript("bv1qx8gvsjy6f7m9p4vrsfzh7atwk8frys2twx4h40");
    CScript shaniScript    = makeScript("bv1q36cjtgj74r4yg3yj4u2j2mspt2uwcllsu8y2cl");
    CScript bharavScript   = makeScript("bv1q52sml5luuv0ghnwdvfxwqpvfj629vk6g32my52");
    CScript braspathScript = makeScript("bv1qck76fwjeykwqut4tkr9ste6hrjj60k68s74njl");
    CScript ganeshScript   = makeScript("bv1qgw4et080p4zf2caqvshk8mehslexh67ulprzs7");

    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << std::vector<unsigned char>{0x2f,0x42,0x69,0x74,0x56,0x61,0x75,0x6c,0x74,0x3a,0x76,0x31,0x2f} << OP_0; // BitVault coinbase tag
    coinbaseTx.vout.clear();
    // MINING PAUSED: miner reward goes to null (burned) until external miners connect
    // To re-enable: replace nullScript with scriptPubKeyIn
    CScript nullScript; // empty script = provably unspendable (OP_RETURN style burn)
    coinbaseTx.vout.emplace_back(nMinerReward,     scriptPubKeyIn);    // vout[0]  10% miner ACTIVE
    coinbaseTx.vout.emplace_back(nAppsReward,      appsScript);       // vout[1]  50% APPS
    coinbaseTx.vout.emplace_back(nOwnerAReward,    ownerAScript);     // vout[2]   5% OWNER_A
    coinbaseTx.vout.emplace_back(nOwnerBReward,    ownerBScript);     // vout[3]   5% OWNER_B
    coinbaseTx.vout.emplace_back(nOwnerCReward,    ownerCScript);     // vout[4]   5% OWNER_C
    coinbaseTx.vout.emplace_back(nOwnerDReward,    ownerDScript);     // vout[5]   5% OWNER_D
    coinbaseTx.vout.emplace_back(nOwnerEReward,    ownerEScript);     // vout[6]   5% OWNER_E
    coinbaseTx.vout.emplace_back(nCharity1Reward,  charity1Script);   // vout[7]   1% CHARITY_1
    coinbaseTx.vout.emplace_back(nCharity2Reward,  charity2Script);   // vout[8]   1% CHARITY_2
    coinbaseTx.vout.emplace_back(nCharity3Reward,  charity3Script);   // vout[9]   1% CHARITY_3
    coinbaseTx.vout.emplace_back(nCharity4Reward,  charity4Script);   // vout[10]  1% CHARITY_4
    coinbaseTx.vout.emplace_back(nCharity5Reward,  charity5Script);   // vout[11]  1% CHARITY_5
    coinbaseTx.vout.emplace_back(nSaavariyaReward, saavariyaScript);  // vout[12]  1% SAAVARIYA
    coinbaseTx.vout.emplace_back(nGovindReward,    govindScript);     // vout[13]  1% GOVIND
    coinbaseTx.vout.emplace_back(nBajrangReward,   bajrangScript);    // vout[14]  1% BAJRANG
    coinbaseTx.vout.emplace_back(nKhatuReward,     khatuScript);      // vout[15]  1% KHATU
    coinbaseTx.vout.emplace_back(nNarsingReward,   narsingScript);    // vout[16]  1% NARSING
    coinbaseTx.vout.emplace_back(nShivReward,      shivScript);       // vout[17]  1% SHIV
    coinbaseTx.vout.emplace_back(nShaniReward,     shaniScript);      // vout[18]  1% SHANI
    coinbaseTx.vout.emplace_back(nBharavReward,    bharavScript);     // vout[19]  1% BHARAV
    coinbaseTx.vout.emplace_back(nBraspathReward,  braspathScript);   // vout[20]  1% BRASPATH
    coinbaseTx.vout.emplace_back(nGaneshReward,    ganeshScript);     // vout[21]  1%+dust GANESH

    // BitVault: one-time premine activation (see bitvault_activation.h)
    if (bitvault::IsPremineActivationBlock(m_chainstate.m_chainman.GetParams().GetChainType(), nHeight)) {
        for (const auto& [value, script] : bitvault::PremineActivationOutputs()) {
            coinbaseTx.vout.emplace_back(value, script);
        }
        LogPrintf("BitVault: premine activation outputs added to block %d\n", nHeight);
    }

    LogPrintf("Coinbase: miner=%lld apps=%lld owners5=%lld charity5=%lld team10=%lld total=%lld\n", nMinerReward, nAppsReward, nOwnerAReward*5, nCharity1Reward*5, nSaavariyaReward*10, nTotalReward);

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vchCoinbaseCommitment = m_chainstate.m_chainman.GenerateCoinbaseCommitment(*pblock, pindexPrev);
    pblocktemplate->vTxFees[0] = -nFees;

    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    // === Kaspa-style BlockDAG: populate parent blocks from DAG tips ===
    pblock->hashParentBlocks.clear();
    auto dagTips = m_chainstate.m_chainman.m_ghostdag.GetTips();
    for (const auto& tip : dagTips) {
        if (tip != pblock->hashPrevBlock) pblock->hashParentBlocks.push_back(tip);
    }
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev, algo);
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus(), algo);
    pblock->nNonce         = 0;
    // === Proof of History ===
    pblock->nPoHIterations = 1000;
    PoHProof pohProof = GeneratePoH(pblock->hashPrevBlock, pblock->nPoHIterations);
    pblock->hashPoH = pohProof.hash;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);


    BlockValidationState state;
    if (m_options.test_block_validity && !TestBlockValidity(state, chainparams, m_chainstate, *pblock, pindexPrev,
                                                  GetAdjustedTime, /*fCheckPOW=*/false, /*fCheckMerkleRoot=*/false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, state.ToString()));
    }
    const auto time_2{SteadyClock::now()};

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n",
             Ticks<MillisecondsDouble>(time_1 - time_start), nPackagesSelected, nDescendantsUpdated,
             Ticks<MillisecondsDouble>(time_2 - time_1),
             Ticks<MillisecondsDouble>(time_2 - time_start));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        } else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= m_options.nBlockMaxWeight) {
        return false;
    }
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST) {
        return false;
    }
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package) const
{
    for (CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, m_lock_time_cutoff)) {
            return false;
        }
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblocktemplate->block.vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee rate %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

/** Add descendants of given transactions to mapModifiedTx with ancestor
 * state updated assuming given transactions are inBlock. Returns number
 * of updated descendants. */
static int UpdatePackagesForAdded(const CTxMemPool& mempool,
                                  const CTxMemPool::setEntries& alreadyAdded,
                                  indexed_modified_transaction_set& mapModifiedTx) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs)
{
    AssertLockHeld(mempool.cs);

    int nDescendantsUpdated = 0;
    for (CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc)) {
                continue;
            }
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                mit = mapModifiedTx.insert(modEntry).first;
            }
            mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
        }
    }
    return nDescendantsUpdated;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(const CTxMemPool& mempool, int& nPackagesSelected, int& nDescendantsUpdated)
{
    AssertLockHeld(mempool.cs);

    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty()) {
        // First try to find a new transaction in mapTx to evaluate.
        //
        // Skip entries in mapTx that are already in a block or are present
        // in mapModifiedTx (which implies that the mapTx ancestor state is
        // stale due to ancestor inclusion in the block)
        // Also skip transactions that we've already failed to add. This can happen if
        // we consider a transaction in mapModifiedTx and it fails: we can then
        // potentially consider it again while walking mapTx.  It's currently
        // guaranteed to fail again, but as a belt-and-suspenders check we put it in
        // failedTx and avoid re-evaluation, since the re-evaluation would be using
        // cached size/sigops/fee values that are not actually correct.
        /** Return true if given transaction from mapTx has already been evaluated,
         * or if the transaction's cached data in mapTx is incorrect. */
        if (mi != mempool.mapTx.get<ancestor_score>().end()) {
            auto it = mempool.mapTx.project<0>(mi);
            assert(it != mempool.mapTx.end());
            if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it)) {
                ++mi;
                continue;
            }
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == m_mempool->mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = m_mempool->mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < m_options.blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    m_options.nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        auto ancestors{mempool.AssumeCalculateMemPoolAncestors(__func__, *iter, CTxMemPool::Limits::NoLimits(), /*fSearchForParents=*/false)};

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, sortedEntries);

        for (size_t i = 0; i < sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(mempool, ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << std::vector<unsigned char>{0x2f,0x42,0x69,0x74,0x56,0x61,0x75,0x6c,0x74,0x3a,0x76,0x31,0x2f} << CScriptNum(nExtraNonce)); // BitVault coinbase tag
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
}

} // namespace node
