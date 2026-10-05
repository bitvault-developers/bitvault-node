// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_PRIMITIVES_BLOCK_H
#define BITVAULT_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <crypto/poh.h>
#include <util/time.h>

namespace Consensus { struct Params; }

enum {
    ALGO_UNKNOWN = -1,
    ALGO_SHA256D  = 0,
    ALGO_SCRYPT   = 1,
    ALGO_GROESTL  = 2,
    ALGO_SKEIN    = 3,
    ALGO_QUBIT    = 4,
    ALGO_EQUIHASH = 5,
    ALGO_ETHASH   = 6,
    ALGO_ODO      = 7,
    ALGO_RANDOMX  = 8,
    ALGO_VERSAHASH = 9,
    ALGO_KHEAVYHASH = 10,
    NUM_ALGOS_IMPL };

const int NUM_ALGOS = NUM_ALGOS_IMPL;

enum {
    // primary version
    BLOCK_VERSION_DEFAULT        = 2, 

    // algo
    BLOCK_VERSION_ALGO           = (31 << 8), // expanded to cover algos 0-18
    BLOCK_VERSION_SCRYPT         = (0 << 8),
    BLOCK_VERSION_SHA256D        = (2 << 8),
    BLOCK_VERSION_GROESTL        = (4 << 8),
    BLOCK_VERSION_SKEIN          = (6 << 8),
    BLOCK_VERSION_QUBIT          = (8 << 8),
    BLOCK_VERSION_EQUIHASH       = (10 << 8),
    BLOCK_VERSION_ETHASH         = (12 << 8),
    BLOCK_VERSION_ODO            = (14 << 8),
    BLOCK_VERSION_RANDOMX        = (16 << 8),
    BLOCK_VERSION_VERSAHASH      = (18 << 8),
    BLOCK_VERSION_KHEAVYHASH    = (20 << 8),
};

std::string GetAlgoName(int Algo);

int GetAlgoByName(std::string strAlgo, int fallback);

inline int GetVersionForAlgo(int algo)
{
    switch(algo)
    {
        case ALGO_SHA256D:
            return BLOCK_VERSION_SHA256D;
        case ALGO_SCRYPT:
            return BLOCK_VERSION_SCRYPT;
        case ALGO_GROESTL:
            return BLOCK_VERSION_GROESTL;
        case ALGO_SKEIN:
            return BLOCK_VERSION_SKEIN;
        case ALGO_QUBIT:
            return BLOCK_VERSION_QUBIT;
        case ALGO_EQUIHASH:
            return BLOCK_VERSION_EQUIHASH;
        case ALGO_ETHASH:
            return BLOCK_VERSION_ETHASH;
        case ALGO_ODO:
            return BLOCK_VERSION_ODO;
        case ALGO_RANDOMX:
            return BLOCK_VERSION_RANDOMX;
        case ALGO_VERSAHASH:
            return BLOCK_VERSION_VERSAHASH;
        case ALGO_KHEAVYHASH:
            return BLOCK_VERSION_KHEAVYHASH;
        default:
            assert(false);
            return 0;
    }
}

uint32_t OdoKey(const Consensus::Params& params, uint32_t nTime);

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    uint256 hashPoH;
    uint32_t nPoHIterations;
    std::vector<uint256> hashParentBlocks; // Kaspa-style DAG: multiple parent block hashes
    CBlockHeader()
    {

        SetNull();
    }

    SERIALIZE_METHODS(CBlockHeader, obj) { READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nNonce, obj.hashPoH, obj.nPoHIterations, obj.hashParentBlocks); }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        hashPoH.SetNull();
        nPoHIterations = 0;
        hashParentBlocks.clear();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    // Set Algo to use
    inline void SetAlgo(int algo)
    {
        nVersion |= GetVersionForAlgo(algo);
    }
    
    int GetAlgo() const;

    uint256 GetHash() const;

    uint256 GetPoWAlgoHash(const Consensus::Params& params) const;

    NodeSeconds Time() const
    {
        return NodeSeconds{std::chrono::seconds{nTime}};
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<unsigned char> vchBlockSig; // DPoS validator signature
    std::vector<CTransactionRef> vtx;

    // Memory-only flags for caching expensive checks
    mutable bool fChecked;                            // CheckBlock()
    mutable bool m_checked_witness_commitment{false}; // CheckWitnessCommitment()
    mutable bool m_checked_merkle_root{false};        // CheckMerkleRoot()

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    SERIALIZE_METHODS(CBlock, obj)
    {
        READWRITE(AsBase<CBlockHeader>(obj), obj.vtx, obj.vchBlockSig);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        vchBlockSig.clear();
        fChecked = false;
        m_checked_witness_commitment = false;
        m_checked_merkle_root = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.hashPoH        = hashPoH;
        block.nPoHIterations = nPoHIterations;
        block.hashParentBlocks = hashParentBlocks;
        return block;
    }

    std::string ToString(const Consensus::Params& params) const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    /** Historically CBlockLocator's version field has been written to network
     * streams as the negotiated protocol version and to disk streams as the
     * client version, but the value has never been used.
     *
     * Hard-code to the highest protocol version ever written to a network stream.
     * SerParams can be used if the field requires any meaning in the future,
     **/
    static constexpr int DUMMY_VERSION = 70016;

    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(std::vector<uint256>&& have) : vHave(std::move(have)) {}

    SERIALIZE_METHODS(CBlockLocator, obj)
    {
        int nVersion = DUMMY_VERSION;
        READWRITE(nVersion);
        READWRITE(obj.vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITVAULT_PRIMITIVES_BLOCK_H
