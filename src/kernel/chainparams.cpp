// Copyright (c) 2010 Satoshi Nakamoto
#include <pubkey.h>
#include <key.h>
#include <util/strencodings.h>
// Copyright (c) 2009-2021 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <kernel/chainparams.h>
#include <bitvault_validator.h>
#include <limits>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <key_io.h>
#include <script/standard.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>
#include <arith_uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "USA Today: 10/Jan/2014, Target: Data stolen from up to 110M customers";
    const CScript genesisOutputScript = CScript() << 0x0 << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}


/**
 * Build a genesis block with MULTIPLE outputs (premine to multiple addresses).
 * Used for mainnet to allocate initial supply across treasury/staking/dev/marketing/reserve.
 */
static CBlock CreateGenesisBlockMultiOutput(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion,
                                             const std::vector<std::pair<CAmount, CScript>>& outputs)
{
    const char* pszTimestamp = "USA Today: 10/Jan/2014, Target: Data stolen from up to 110M customers";
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(outputs.size());
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    for (size_t i = 0; i < outputs.size(); ++i) {
        txNew.vout[i].nValue = outputs[i].first;
        txNew.vout[i].scriptPubKey = outputs[i].second;
    }

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();

        // ----- CUSTOM HALVING INTERVAL -----
        consensus.nSubsidyHalvingInterval = 5259600; // 10 years (60s blocks)

        // ----- BITVAULT MAINNET ACTIVATION HEIGHTS -----
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0xadd8ca420f557f62377ec2be6e6f47b96cf2e68160d58aeb7b73433de834cca0");
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 0;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 483840;
        consensus.ReserveAlgoBitsHeight = 8547840;
        consensus.OdoHeight = 9112320;

        // ----- PROOF-OF-WORK LIMITS -----
        consensus.powLimit = ArithToUint256(~arith_uint256(0) >> 20);
        consensus.initialTarget[ALGO_ODO] = ArithToUint256(~arith_uint256(0) >> 40);

        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60; // 60 seconds
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fEasyPow = false;
        consensus.fPowNoRetargeting = false;
        consensus.fPoB = true; // Enable Proof of BitVault for mainnet
        consensus.nPow80ActivationTime = BVT_POW80_ACTIVATION_TIME; // BVT_POW80
        consensus.bvtValidatorPubKey = BVT_VALIDATOR_PUBKEY; // BVT_POW80
        consensus.fRbfEnabled = false;

        // ----- MULTI‑ALGO AND DIFFICULTY PARAMETERS -----
        consensus.nOdoShapechangeInterval = 10*24*60*60; // 10 days
        consensus.nRuleChangeActivationThreshold = 28224; // 70% of 40320 blocks
        consensus.nMinerConfirmationWindow = 40320; // 1 week
        consensus.MinBIP9WarningHeight = 9152640;

        consensus.multiAlgoDiffChangeTarget = 145000;
        consensus.alwaysUpdateDiffChangeTarget = 400000;
        consensus.workComputationChangeTarget = 1430000;
        consensus.algoSwapChangeTarget = 9100000;
        consensus.nTargetTimespan = 0.10 * 24 * 60 * 60; // 2.4 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 67200;
        consensus.patchBlockRewardDuration = 10080;
        consensus.patchBlockRewardDuration2 = 80160;
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block
        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 60*11; // NUM_ALGOS * 60 sec
        consensus.multiAlgoTargetSpacingV4 = 60*11; // NUM_ALGOS * 60 sec
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing;
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4;
        consensus.nMaxAdjustDown = 40;
        consensus.nMaxAdjustUp = 20;
        consensus.nMaxAdjustDownV3 = 16;
        consensus.nMaxAdjustUpV3 = 8;
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4;
        consensus.nLocalDifficultyAdjustment = 4;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;

        // Deployment of Taproot
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1736510438; // 10th January 2025
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1799582438; // 10th January 2027
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;

        consensus.nMinimumChainWork = uint256S("0x00"); // BVT: left at 0 - header pre-sync rejects per-block multi-algo difficulty changes
        consensus.defaultAssumeValid = uint256S("0x0408baec249a996c68fedf275ed7949aa4b15e2bd57a5b3ed3c860df56b483bd"); // h=25000 Oct 1 2026

        // ===== NETWORK IDENTIFIERS =====
        pchMessageStart[0] = 0xfb;
        pchMessageStart[1] = 0xc0;
        pchMessageStart[2] = 0xc1;
        pchMessageStart[3] = 0xdb;

        nDefaultPort = 29433;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 32;
        m_assumed_chain_state_size = 1;

        // ===== GENESIS BLOCK with PREMINE =====
        // Premine: 50,000,000,000 BVT (50B) split equally across 10 mainnet addresses
        // - 10 wallets x 5,000,000,000 BVT each = 50B BVT total
        // - Wallets generated from "mainminer" wallet on May 3, 2026
        // - All addresses are P2WPKH (bech32 bv1q...) under hardened BIP32 derivation
        // Build P2WPKH scripts directly from hash160 (avoids DecodeDestination,
        // which would require globalChainParams that doesn't exist yet during construction)
        auto makeP2WPKH = [](const char* hex) {
            std::vector<unsigned char> hash160 = ParseHex(hex);
            CScript script;
            script << OP_0 << hash160;
            return script;
        };
        std::vector<std::pair<CAmount, CScript>> premineOutputs = {
            { 5'000'000'000LL * COIN, makeP2WPKH("eda392a8cceed3d01b3e604880b3653482105c9d") }, // PREMINE_1  -> bv1qak3e92xvamfaqxe7vpygpvm9xjppqhya4kpxzq
            { 5'000'000'000LL * COIN, makeP2WPKH("8770326d3444113f235854211b0c784b2aa88711") }, // PREMINE_2  -> bv1qsacrymf5gsgn7g6c2ss3krrcfv423pc3dvhq2k
            { 5'000'000'000LL * COIN, makeP2WPKH("9b3b81217512d267ccba1b0f40b6df43ca2c4fd4") }, // PREMINE_3  -> bv1qnvaczgt4ztfx0n96rv85pdklg09zcn75u5lpaj
            { 5'000'000'000LL * COIN, makeP2WPKH("ab89685343cfdcb43eb328368e449c5f5e4e2304") }, // PREMINE_4  -> bv1q4wyks56relwtg04n9qmgu3yuta0yugcyxsqq6s
            { 5'000'000'000LL * COIN, makeP2WPKH("8f9139ac544024e3345ffba68ec41d0121f3ea8b") }, // PREMINE_5  -> bv1q37gnntz5gqjwxdzllwnga3qaqysl865tvcu35j
            { 5'000'000'000LL * COIN, makeP2WPKH("6a54bcc47670a983969475f4984dd65feffbce57") }, // PREMINE_6  -> bv1qdf2te3rkwz5c8955wh6fsnwktlhlhnjhlc99e6
            { 5'000'000'000LL * COIN, makeP2WPKH("489d7168b21673f8260126b3ebb79f764fe92cf7") }, // PREMINE_7  -> bv1qfzwhz69jzeelsfspy6e7hdulwe87jt8hs2df2q
            { 5'000'000'000LL * COIN, makeP2WPKH("0c056423500add16d449a3dfc7e214ebbe1c6dc9") }, // PREMINE_8  -> bv1qpszkgg6sptw3d4zf500u0cs5awlpcmwfx03zak
            { 5'000'000'000LL * COIN, makeP2WPKH("298ba818052408b0db1ab584078603e12153c08f") }, // PREMINE_9  -> bv1q9x96sxq9ysytpkc6kkzq0psruys48sy0u6n8sc
            { 5'000'000'000LL * COIN, makeP2WPKH("0a2040d1c6f3cd552aaf679d56c9e57f3ecab8b1") }, // PREMINE_10 -> bv1qpgsyp5wx70x42240v7w4dj090ulv4w93yt4ghh
        };
        genesis = CreateGenesisBlockMultiOutput(
            /* nTime */    1777200000,
            /* nNonce */   1319900,
            /* nBits */    0x1e0fffff,
            /* nVersion */ 1,
            premineOutputs
        );
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xa895abd339dd3ff5e777f92ca892ec24f566e73da08779eb82264cd146bd06db"));
        assert(genesis.hashMerkleRoot == uint256S("0x07020e81d68814b13c0cfd1409b9a0f2b13ee5f1d1699063d06849661549a2eb"));

        // ===== SEEDS =====
        vSeeds.emplace_back("seed.bitvault.club");
        vSeeds.emplace_back("dnsseed.bitvault.club");
        // BitVault Tor onion node (always reachable, ISP-bypass)
        vSeeds.emplace_back("3ctstbkthlxkhjz6gg6nkshf27u3plsycduqfmhnewjeqboatf6u45yd.onion:29433");
        // BitVault founder node (dynamic IP, updated via DNS)
        vSeeds.emplace_back("106.219.70.176:29433");

        // ===== ADDRESS PREFIXES =====
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,50);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,110);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,176);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "bv";

        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                { 0,  uint256S("0xa895abd339dd3ff5e777f92ca892ec24f566e73da08779eb82264cd146bd06db") }, // genesis
                { 25000, uint256S("0x0408baec249a996c68fedf275ed7949aa4b15e2bd57a5b3ed3c860df56b483bd") }, // h=25000 Oct 1 2026 - BVT history checkpoint
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            1777200000, // genesis timestamp (BitVault mainnet genesis)
            0,
            0.0
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 300;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105"), SCRIPT_VERIFY_NONE);
        consensus.BIP34Height = 500; // BIP34 activated on testnet (Used in functional tests)
        consensus.BIP34Hash = uint256S("0x0");
        consensus.BIP65Height = 1351; // BIP65 activated on testnet (Used in functional tests)
        consensus.BIP66Height = 1251; // BIP66 activated on testnet (Used in functional tests)
        consensus.CSVHeight = 1; // CSV activated on testnet (Used in rpc activation tests)
        consensus.SegwitHeight = 0; // SEGWIT is always activated on testnet unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = ArithToUint256(~arith_uint256(0) >> 20);
        consensus.initialTarget[ALGO_ODO] = ArithToUint256(~arith_uint256(0) >> 36); // 16 difficulty
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 60 / 4;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fEasyPow = false;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 4032; // 4032 - 70% of 5760
        consensus.nMinerConfirmationWindow = 5760; // 1 day of blocks on testnet
        consensus.fRbfEnabled = false;

        // BitVault Specific Consensus Code (testnet)
        consensus.nTargetTimespan =  0.10 * 24 * 60 * 60; // 2.4 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 67; // DigiShield Hard Fork Block BIP34Height 67,200
        consensus.patchBlockRewardDuration = 10; // Old 1% monthly BVT Reward
        consensus.patchBlockRewardDuration2 = 80; // 4 blocks per min
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block
        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 60*11; // NUM_ALGOS * 60 sec
        consensus.multiAlgoTargetSpacingV4 = 60*11; // NUM_ALGOS * 60 sec
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing;
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4;
        consensus.nMaxAdjustDown = 40; // 40% adjustment down
        consensus.nMaxAdjustUp = 20; // 20% adjustment up
        consensus.nMaxAdjustDownV3 = 16; // 16% adjustment down
        consensus.nMaxAdjustUpV3 = 8; // 8% adjustment up
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4; // target adjustment per algo
        consensus.nLocalDifficultyAdjustment = 4; // difficulty adjustment per algo

        // Hard Fork Block Heights for testnet
        consensus.multiAlgoDiffChangeTarget = 100; // Block 145,000 MultiAlgo Hard Fork
        consensus.alwaysUpdateDiffChangeTarget = 400; // Block 400,000 MultiShield Hard Fork
        consensus.workComputationChangeTarget = 1430; // Block 1,430,000 DigiSpeed Hard Fork
        consensus.algoSwapChangeTarget = 20000; // Block 9,000,000 Odo PoW Hard Fork
        consensus.OdoHeight = 0;
        consensus.nHybridStartHeight = std::numeric_limits<int>::max(); // disabled for regtest
        consensus.ReserveAlgoBitsHeight = 0;
        consensus.nOdoShapechangeInterval = 1*24*60*60; // 1 day
        consensus.fPoB = true;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1718921304; // 20th June 2024 Testnet
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1750457304; // 20th June 2025 Testnet
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00"); //1079274

        pchMessageStart[0] = 0xfd;
        pchMessageStart[1] = 0xc8;
        pchMessageStart[2] = 0xbd;
        pchMessageStart[3] = 0xdd;
        nDefaultPort = 12026; // BitVault testnet
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 40;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlock(1516939474, 2411473, 0x1e0ffff0, 1, 8000);
        consensus.hashGenesisBlock = genesis.GetHash();
        // assert(consensus.hashGenesisBlock == uint256S("0x308ea0711d5763be2995670dd9ca9872753561285a84da1d58be58acaa822252"));
        // assert(genesis.hashMerkleRoot == uint256S("0x72ddd9496b004221ed0557358846d9248ecd4c440ebd28ed901efc18757d0fad"));

        vFixedSeeds.clear();
        vSeeds.clear();

        // BitVault TESTNET DNS Seed Servers:
        vSeeds.emplace_back("seed-testnet.bitvault.network"); // Olly Stedall @saltedlolly
        vSeeds.emplace_back("seed2-testnet.bitvault.network"); // John Song @j50ng
        vSeeds.emplace_back("seed3-testnet.bitvault.network"); // Jan De Jong @jongjan88
        vSeeds.emplace_back("seed4-testnet.bitvault.network"); // Bastian Driessen @bastiandriessen
        vSeeds.emplace_back("seed5-testnet.bitvault.network"); // Craig Donnachie @cdonnachie

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,126);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,140);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,254);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bvt";

        vFixedSeeds = std::vector<uint8_t>(chainparams_seed_test, chainparams_seed_test + sizeof(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {   546, uint256S("0x08fa50178f4b4f9fe1bbaed3b0a2ee58d1c51cc8185f70c8089e4b95763d9cdb")},
            }
        };

        m_assumeutxo_data = {
            // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{
            .nTime    = 1700000000,  // Approximate November 2023
            .nTxCount = 1000000,     // Approximate testnet transactions
            .dTxRate  = 0.01,        // Lower rate for testnet
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vSeeds.clear();

        if (!options.challenge) {
            bin = ParseHex("512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae");
            vSeeds.emplace_back("seed.signet.bitvault.sprovoost.nl.");

            // Hardcoded nodes can be removed once there are more DNS seeds
            vSeeds.emplace_back("178.128.221.177");
            vSeeds.emplace_back("v7ajjeirttkbnt32wpy3c6w3emwnfr3fkla7hpxcfokr3ysd3kqtzmqd.onion:38333");

            consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000001ad46be4862");
            consensus.defaultAssumeValid = uint256S("0x0000013d778ba3f914530f11f6b69869c9fab54acff85acd7b8201d111f19b7f"); // 150000
            m_assumed_blockchain_size = 1;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                .nTime    = 1688366339,
                .nTxCount = 2262750,
                .dTxRate  = 0.003414084572046456,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 300; // BitVault halving interval for signet (same as testnet)
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 1; // 1 second (Kaspa-style)
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fEasyPow = false;
        consensus.fPowNoRetargeting = false;
        consensus.fRbfEnabled = false;

        // BitVault Specific Consensus Code for signet (same as testnet)
        consensus.nOdoShapechangeInterval = 1*24*60*60; // 1 day
        consensus.nTargetTimespan =  0.10 * 24 * 60 * 60; // 2.4 hours
        consensus.nTargetSpacing = 60; // 60 seconds
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 67; // DigiShield Hard Fork Block
        consensus.patchBlockRewardDuration = 10;
        consensus.patchBlockRewardDuration2 = 80;
        consensus.nTargetTimespanRe = 1*60; // 60 Seconds
        consensus.nTargetSpacingRe = 1*60; // 60 seconds
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe; // 1 block
        consensus.nAveragingInterval = 10; // 10 blocks
        consensus.multiAlgoTargetSpacing = 60*11; // NUM_ALGOS * 60 sec
        consensus.multiAlgoTargetSpacingV4 = 60*11; // NUM_ALGOS * 60 sec
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing;
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4;
        consensus.nMaxAdjustDown = 40; // 40% adjustment down
        consensus.nMaxAdjustUp = 20; // 20% adjustment up
        consensus.nMaxAdjustDownV3 = 16; // 16% adjustment down
        consensus.nMaxAdjustUpV3 = 8; // 8% adjustment up
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4; // target adjustment per algo
        consensus.nLocalDifficultyAdjustment = 4; // difficulty adjustment per algo

        // Hard Fork Block Heights for signet (same as testnet)
        consensus.multiAlgoDiffChangeTarget = 0; // All algos active from genesis on regtest
        consensus.alwaysUpdateDiffChangeTarget = 400; // Block 400 MultiShield Hard Fork
        consensus.workComputationChangeTarget = 1430; // Block 1,430 DigiSpeed Hard Fork
        consensus.algoSwapChangeTarget = 20000; // Block 20,000 Odo PoW Hard Fork
        consensus.OdoHeight = 0;
        consensus.ReserveAlgoBitsHeight = 0;
        consensus.initialTarget[ALGO_ODO] = ArithToUint256(~arith_uint256(0) >> 36); // 16 difficulty

        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("00000377ae000000000000000000000000000000000000000000000000000000");
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 38443;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1598918400, 52613770, 0x1e0377ae, 1, 8000);
        consensus.hashGenesisBlock = genesis.GetHash();
        // assert(consensus.hashGenesisBlock == uint256S("0x9cf8c097b1afc5a37d2d050d2b423b052c2da2856acf0e41c40af1da334fcbf7"));
        // assert(genesis.hashMerkleRoot == uint256S("0x72ddd9496b004221ed0557358846d9248ecd4c440ebd28ed901efc18757d0fad"));

        vFixedSeeds.clear();

        m_assumeutxo_data = {
            {
                .height = 160'000,
                .hash_serialized = AssumeutxoHash{uint256S("0xfe0a44309b74d6b5883d246cb419c6221bcccf0b308c9b59b7d70783dbdf928a")},
                .nChainTx = 2289496,
                .blockhash = uint256S("0x0000003ca3c99aff040f2563c2ad8f8ec88bd0fd6b8f0895cfaf1ef90353a62c")
            }
        };

        // Use same prefixes as testnet for signet
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,126);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,140);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,254);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bvt";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();

        consensus.nSubsidyHalvingInterval = 21024000; // 10 years (15 sec blocks)

        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;

        consensus.ReserveAlgoBitsHeight = 0;
        consensus.OdoHeight = 0;
        consensus.MinBIP9WarningHeight = 0;

        // Proof‑of‑work limits – easy regtest difficulty
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // Set initial targets for all algorithms
        consensus.initialTarget[ALGO_SHA256D] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_SCRYPT] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_GROESTL] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_SKEIN] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_QUBIT] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.initialTarget[ALGO_ODO] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        // Target spacing – keep multi‑algo values
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
        consensus.nPowTargetSpacing = 1; // 1 second (Kaspa-style)

        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fEasyPow = true;
        consensus.fPowNoRetargeting = true;
        consensus.fPoB = true; // Enable Proof of BitVault (DPoS signing)
        consensus.nPow80ActivationTime = 0; // BVT_POW80: active from genesis on regtest
        consensus.bvtValidatorPubKey = BVT_REGTEST_VALIDATOR_PUBKEY; // BVT_POW80
        consensus.fRbfEnabled = false;

        // ===== KEEP ALL MULTI‑ALGO PARAMETERS =====
        consensus.nOdoShapechangeInterval = 10*24*60*60;
        consensus.nTargetTimespan = 0.10 * 48 * 60 * 60;
        consensus.nTargetSpacing = 1; // 1 sec Kaspa-style
        consensus.nInterval = consensus.nTargetTimespan / consensus.nTargetSpacing;
        consensus.nDiffChangeTarget = 334;
        consensus.patchBlockRewardDuration = 10;
        consensus.patchBlockRewardDuration2 = 80;
        consensus.nTargetTimespanRe = 1*60;
        consensus.nTargetSpacingRe = 1; // 1 sec retarget
        consensus.nIntervalRe = consensus.nTargetTimespanRe / consensus.nTargetSpacingRe;
        consensus.nAveragingInterval = 10;
        consensus.multiAlgoTargetSpacing = 1*11; // NUM_ALGOS_IMPL * 1 sec
        consensus.multiAlgoTargetSpacingV4 = 1*11; // NUM_ALGOS_IMPL * 1 sec
        consensus.nAveragingTargetTimespan = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacing;
        consensus.nAveragingTargetTimespanV4 = consensus.nAveragingInterval * consensus.multiAlgoTargetSpacingV4;
        consensus.nMaxAdjustDown = 40;
        consensus.nMaxAdjustUp = 20;
        consensus.nMaxAdjustDownV3 = 16;
        consensus.nMaxAdjustUpV3 = 8;
        consensus.nMaxAdjustDownV4 = 16;
        consensus.nMaxAdjustUpV4 = 8;
        consensus.nMinActualTimespan = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUp) / 100;
        consensus.nMaxActualTimespan = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustDown) / 100;
        consensus.nMinActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 - consensus.nMaxAdjustUpV3) / 100;
        consensus.nMaxActualTimespanV3 = consensus.nAveragingTargetTimespan * (100 + consensus.nMaxAdjustUpV3) / 100;
        consensus.nMinActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 - consensus.nMaxAdjustUpV4) / 100;
        consensus.nMaxActualTimespanV4 = consensus.nAveragingTargetTimespanV4 * (100 + consensus.nMaxAdjustUpV4) / 100;
        consensus.nLocalTargetAdjustment = 4;
        consensus.nLocalDifficultyAdjustment = 4;

        consensus.multiAlgoDiffChangeTarget = 1000000;
        consensus.alwaysUpdateDiffChangeTarget = 2000000;
        consensus.workComputationChangeTarget = 3000000;
        consensus.algoSwapChangeTarget = 4000000;

        consensus.nRuleChangeActivationThreshold = 108;
        consensus.nMinerConfirmationWindow = 144;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 27;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        // ===== NETWORK IDENTIFIERS =====
        pchMessageStart[0] = 0xfb;
        pchMessageStart[1] = 0xc0;
        pchMessageStart[2] = 0xc1;
        pchMessageStart[3] = 0xdb;

        nDefaultPort = 29433;

        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        // ===== HANDLE COMMAND‑LINE OVERRIDES =====
        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }
        // ===== REGTEST GENESIS — BitVault 50B Pre-mine =====
        genesis = CreateGenesisBlock(1519460922, 4, 0x207fffff, 1, 50000000000LL * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // New genesis hash with 50B reward — will be printed on first run
        // ===== SEEDS & CHECKPOINTS =====
        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256S("ed6ad60ff9c64ac5a4fa48c4fa10ec014279b732945fc5e941005eb9dd3fc88f")},
            }
        };

        m_assumeutxo_data = {
            {
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256S("0x2da005f8e675e4c37ea7d7266d11d2d9c2485c095fff21692ef299fbba42f87c")},
                .nChainTx = 111,
                .blockhash = uint256S("0x56b2d1cd24ac6d9c74d3f06867eb2d1b1ca1d455f635dac3e1a450da96ed6374")
            },
        };

        chainTxData = ChainTxData{0, 0, 0};

        // ===== ADDRESS PREFIXES =====
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bvrt";
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}
