// Copyright (c) 2014-2026 The BitVault Core developers
// BIP119 OP_CHECKTEMPLATEVERIFY
#ifndef BITVAULT_SCRIPT_CTV_H
#define BITVAULT_SCRIPT_CTV_H

#include <hash.h>
#include <primitives/transaction.h>
#include <uint256.h>

/** BIP119 DefaultCheckTemplateVerifyHash — works for CTransaction and CMutableTransaction */
template <typename T>
uint256 ComputeCTVHash(const T& tx, uint32_t nIn)
{
    HashWriter ss{};
    ss << tx.nVersion;
    ss << tx.nLockTime;
    bool has_scriptsig = false;
    for (const auto& in : tx.vin) {
        if (!in.scriptSig.empty()) { has_scriptsig = true; break; }
    }
    if (has_scriptsig) {
        HashWriter ss2{};
        for (const auto& in : tx.vin) ss2 << in.scriptSig;
        ss << ss2.GetHash();
    }
    {
        HashWriter ss2{};
        for (const auto& in : tx.vin) ss2 << in.nSequence;
        ss << ss2.GetHash();
    }
    ss << (uint32_t)tx.vout.size();
    {
        HashWriter ss2{};
        for (const auto& out : tx.vout) ss2 << out;
        ss << ss2.GetHash();
    }
    ss << nIn;
    return ss.GetHash();
}

#endif // BITVAULT_SCRIPT_CTV_H
