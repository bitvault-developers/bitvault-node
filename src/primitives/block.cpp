// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <primitives/block.h>
#include <crypto/common.h>
#include <crypto/randomx.h>
#include <crypto/hashgroestl.h>
#include <crypto/hashodo.h>
#include <crypto/hashqubit.h>
#include <crypto/hashskein.h>
#include <crypto/ethash_wrapper.h>
#include <crypto/equihash_bv.h>
#include <crypto/versahash.h>
#include <crypto/kheavyhash.h>
#include <crypto/scrypt.h>
#include <consensus/consensus.h>
#include <chainparams.h>
#include <hash.h>
#include <streams.h>
#include <tinyformat.h>
#include <arith_uint256.h>

uint256 CBlockHeader::GetHash() const
{
    return (CHashWriter{PROTOCOL_VERSION} << *this).GetHash();
}

int CBlockHeader::GetAlgo() const
{
    switch (nVersion & BLOCK_VERSION_ALGO)
    {
        case BLOCK_VERSION_SCRYPT:
            return ALGO_SCRYPT;
        case BLOCK_VERSION_SHA256D:
            return ALGO_SHA256D;
        case BLOCK_VERSION_GROESTL:
            return ALGO_GROESTL;
        case BLOCK_VERSION_SKEIN:
            return ALGO_SKEIN;
        case BLOCK_VERSION_QUBIT:
            return ALGO_QUBIT;
        case BLOCK_VERSION_EQUIHASH:
            return ALGO_EQUIHASH;
        case BLOCK_VERSION_ETHASH:
            return ALGO_ETHASH;
        case BLOCK_VERSION_ODO:
            return ALGO_ODO;
        case BLOCK_VERSION_RANDOMX:
            return ALGO_RANDOMX;
        case BLOCK_VERSION_VERSAHASH:
            return ALGO_VERSAHASH;
        case BLOCK_VERSION_KHEAVYHASH:
            return ALGO_KHEAVYHASH;
    }
    return ALGO_UNKNOWN;
}

uint32_t OdoKey(const Consensus::Params& params, uint32_t nTime)
{
    uint32_t nShapechangeInterval = params.nOdoShapechangeInterval;
    return nTime - nTime % nShapechangeInterval;

}

// BVT_POW80: the 80-byte base header (version, prev, merkle, time, bits, nonce) that standard ASICs and Stratum hash.
static uint256 BvtBaseHeaderHash(const CBlockHeader& h)
{
    CHashWriter hw{PROTOCOL_VERSION};
    hw << h.nVersion << h.hashPrevBlock << h.hashMerkleRoot << h.nTime << h.nBits << h.nNonce;
    return hw.GetHash();
}

uint256 CBlockHeader::GetPoWAlgoHash(const Consensus::Params& params) const
{
    const bool pow80 = static_cast<int64_t>(nTime) >= params.nPow80ActivationTime; // BVT_POW80
    switch (GetAlgo())
    {
        case ALGO_SHA256D:
            return pow80 ? BvtBaseHeaderHash(*this) : GetHash(); // BVT_POW80
        case ALGO_SCRYPT:
        {
            uint256 thash;
            DataStream ss{};
            ss << *this;
            scrypt_1024_1_1_256(reinterpret_cast<const char*>(ss.data()), reinterpret_cast<char*>(thash.data()));
            return thash;
        }
        case ALGO_GROESTL:
        {
            DataStream ss{};
            if (pow80) ss << nVersion << hashPrevBlock << hashMerkleRoot << nTime << nBits << nNonce; else ss << *this; // BVT_POW80
            return HashGroestl(ss.begin(), ss.end());
        }
        case ALGO_RANDOMX:
        {
            std::vector<unsigned char> headerBytes;
            CVectorWriter ss(PROTOCOL_VERSION, headerBytes, 0);
            ss << *this;
            return RandomXHash(headerBytes);
        }
        case ALGO_SKEIN:
        {
            DataStream ss{};
            if (pow80) ss << nVersion << hashPrevBlock << hashMerkleRoot << nTime << nBits << nNonce; else ss << *this; // BVT_POW80
            return HashSkein(ss.begin(), ss.end());
        }
        case ALGO_QUBIT:
        {
            DataStream ss{};
            if (pow80) ss << nVersion << hashPrevBlock << hashMerkleRoot << nTime << nBits << nNonce; else ss << *this; // BVT_POW80
            return HashQubit(ss.begin(), ss.end());
        }
        case ALGO_EQUIHASH:
        {
            DataStream ss{};
            ss << *this;
            return HashEquihashBV(MakeUCharSpan(ss));
        }
        case ALGO_ETHASH:
        {
            uint256 ethashResult;
            if (ComputeEthashHash(*this, 0, ethashResult))
                return ethashResult;
            return GetHash();
        }
        case ALGO_VERSAHASH:
        {
            DataStream ss{};
            ss << *this;
            return HashVersaHash(MakeUCharSpan(ss));
        }
        case ALGO_KHEAVYHASH:
        {
            DataStream ss{};
            ss << *this;
            return HashKHeavyHash(MakeUCharSpan(ss));
        }
        case ALGO_ODO:
        {
            uint32_t key = OdoKey(params, nTime);
            DataStream ss{};
            ss << *this;
            unsigned char compact[80];
            memset(compact, 0, 80);
            CSHA256().Write(reinterpret_cast<const unsigned char*>(ss.data()), ss.size()).Finalize(compact);
            CSHA256().Write(compact, 32).Finalize(compact + 32);
            memcpy(compact + 64, (const unsigned char*)&nTime, 4);
            memcpy(compact + 68, (const unsigned char*)&nBits, 4);
            memcpy(compact + 72, (const unsigned char*)&nNonce, 4);
            return HashOdo(compact, compact + 80, key);
        }
        case ALGO_UNKNOWN:
            // This block will be rejected anyway, but returning an always-invalid
            // PoW hash will allow it to be rejected sooner.
            return ArithToUint256(~arith_uint256(0));
    }
    assert(false);
    return GetHash();
}

std::string CBlock::ToString(const Consensus::Params& params) const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, pow_algo=%d, pow_hash=%s, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        GetAlgo(),
        GetPoWAlgoHash(params).ToString(),
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}

std::string GetAlgoName(int Algo)
{
    switch (Algo)
    {
        case ALGO_SHA256D:
            return std::string("sha256d");
        case ALGO_SCRYPT:
            return std::string("scrypt");
        case ALGO_GROESTL:
            return std::string("groestl");
        case ALGO_SKEIN:
            return std::string("skein");
        case ALGO_QUBIT:
            return std::string("qubit");
        case ALGO_EQUIHASH:
            return std::string("equihash");
        case ALGO_ETHASH:
            return std::string("ethash");
        case ALGO_ODO:
            return std::string("odo");
        case ALGO_RANDOMX:
            return std::string("randomx");
        case ALGO_VERSAHASH:
            return std::string("versahash");
        case ALGO_KHEAVYHASH:
            return std::string("kheavyhash");
    }
    return std::string("unknown");
}

int GetAlgoByName(std::string strAlgo, int fallback)
{
    if (strAlgo == "sha256d")
        return ALGO_SHA256D;
    else if (strAlgo == "scrypt")
        return ALGO_SCRYPT;
    else if (strAlgo == "groestl")
        return ALGO_GROESTL;
    else if (strAlgo == "skein")
        return ALGO_SKEIN;
    else if (strAlgo == "qubit")
        return ALGO_QUBIT;
    else if (strAlgo == "equihash")
        return ALGO_EQUIHASH;
    else if (strAlgo == "ethash")
        return ALGO_ETHASH;
    else if (strAlgo == "odo")
        return ALGO_ODO;
    else if (strAlgo == "randomx")
        return ALGO_RANDOMX;
    else if (strAlgo == "versahash")
        return ALGO_VERSAHASH;
    else if (strAlgo == "kheavyhash")
        return ALGO_KHEAVYHASH;
    return fallback;
}
