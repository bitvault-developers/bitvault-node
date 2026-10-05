// BitVault Consensus Unit Tests
#include <boost/test/unit_test.hpp>
#include <primitives/block.h>
#include <dag/ghostdag.h>
#include <consensus/validator.h>
#include <crypto/poh.h>

BOOST_AUTO_TEST_SUITE(bitvault_consensus_tests)

BOOST_AUTO_TEST_CASE(test_num_algos)
{
    BOOST_CHECK_EQUAL(NUM_ALGOS, NUM_ALGOS_IMPL);
    BOOST_CHECK_EQUAL(NUM_ALGOS, 11);
}

BOOST_AUTO_TEST_CASE(test_algo_names)
{
    BOOST_CHECK_EQUAL(GetAlgoName(ALGO_SHA256D), "sha256d");
    BOOST_CHECK_EQUAL(GetAlgoName(ALGO_KHEAVYHASH), "kheavyhash");
    BOOST_CHECK_EQUAL(GetAlgoName(ALGO_SCRYPT), "scrypt");
}

BOOST_AUTO_TEST_CASE(test_block_version_algo)
{
    CBlockHeader hdr;
    hdr.SetNull();
    hdr.nVersion = BLOCK_VERSION_DEFAULT;
    hdr.SetAlgo(ALGO_KHEAVYHASH);
    BOOST_CHECK_EQUAL(hdr.GetAlgo(), ALGO_KHEAVYHASH);
}

BOOST_AUTO_TEST_CASE(test_poh_generation)
{
    uint256 seed;
    seed.SetNull();
    uint32_t iters = 100;
    PoHProof proof = GeneratePoH(seed, iters);
    BOOST_CHECK(proof.hash != seed);
    BOOST_CHECK(VerifyPoH(seed, proof.hash, iters));
}

BOOST_AUTO_TEST_CASE(test_ghostdag_basic)
{
    GHOSTDAGManager dag(18);
    uint256 h1; h1.SetNull(); h1.IsNull();
    uint256 genesis = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
    uint256 prev; prev.SetNull();
    std::vector<uint256> parents;
    BOOST_CHECK(dag.AddBlock(genesis, 0, prev, parents, 1000));
    BOOST_CHECK_EQUAL(dag.GetBlockCount(), 1);
    BOOST_CHECK_EQUAL(dag.GetBlueScore(genesis), 1);
    BOOST_CHECK(dag.IsBlue(genesis));
}

BOOST_AUTO_TEST_CASE(test_token_split)
{
    int64_t total = 7200000000000LL; // 72000 BVT
    int64_t miner = (total * 45) / 100;
    int64_t treasury = (total * 40) / 100;
    int64_t staking = total - miner - treasury;
    BOOST_CHECK_EQUAL(miner, 3240000000000LL);
    BOOST_CHECK_EQUAL(treasury, 2880000000000LL);
    BOOST_CHECK_EQUAL(staking, 1080000000000LL);
    BOOST_CHECK_EQUAL(miner + treasury + staking, total);
}

BOOST_AUTO_TEST_SUITE_END()
