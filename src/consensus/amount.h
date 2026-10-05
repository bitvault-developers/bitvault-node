// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_CONSENSUS_AMOUNT_H
#define BITVAULT_CONSENSUS_AMOUNT_H

#include <cstdint>

/** Amount in satoshis (Can be negative) */
typedef int64_t CAmount;

/** The amount of satoshis in one BVT. */
static constexpr CAmount COIN = 100000000;

/** No amount larger than this (in satoshi) is valid.
 *
 * Note that this constant is *not* the total money supply, which in BitVault
 * currently happens to be less than 50,000,000,000 BVT for various reasons, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
static constexpr CAmount MAX_MONEY = 9223372036854775807LL; // No hard cap (controlled by halving + supply curve)
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif // BITVAULT_CONSENSUS_AMOUNT_H
