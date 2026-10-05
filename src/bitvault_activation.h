// Copyright (c) 2026 The BitVault Core developers
// Distributed under the MIT software license.

#ifndef BITVAULT_ACTIVATION_H
#define BITVAULT_ACTIVATION_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <utility>
#include <vector>

/**
 * One-time premine activation (mainnet hard fork).
 *
 * The mainnet genesis block records a 50,000,000,000 BVT premine in ten 5B
 * outputs. In Bitcoin-derived chains the genesis coinbase is never added to
 * the UTXO set, so those outputs can never be spent. At
 * PREMINE_ACTIVATION_HEIGHT the premine is issued once, as ten additional
 * coinbase outputs paying cold-storage addresses.
 *
 * Every block below that height validates exactly as before. At that height
 * the coinbase must contain all ten outputs (exact value and script, in any
 * position), and may pay their total on top of the normal block reward.
 */
namespace bitvault {

static constexpr int PREMINE_ACTIVATION_HEIGHT = 20085;

static constexpr int PREMINE_OUTPUT_COUNT = 10;
static constexpr CAmount PREMINE_OUTPUT_VALUE = CAmount{5'000'000'000} * COIN;

// P2WPKH witness programs of PREMINE_COLD_01..10 — cold wallet created
// 2026-09-21, backup fingerprint ea6af0dba2621a50.
static const char* const PREMINE_WITNESS_PROGRAMS[PREMINE_OUTPUT_COUNT] = {
    "e1595503a0df32723b56c09893cdb795fd2141b0", // bv1qu9v42qaqmue8yw6kczvf8ndhjh7jzsdsaanuzh
    "b41841cf28cfb915b09b5da2c856adf9eff2019d", // bv1qksvyrnege7u3tvymtk3vs44dl8hlyqvaquy2v5
    "d265b0502afe27439e63a294ae3f70e7a24585c8", // bv1q6fjmq5p2lcn588nr5222u0msu73ytpwgqmvjp8
    "13e1b7b20c4bcd20d3e165d16bba0296a8b9c98f", // bv1qz0sm0vsvf0xjp5lpvhgkhwszj65tnjv0wvl8u8
    "40b15942db47307202e34870a79db8101237df6f", // bv1qgzc4jskmguc8yqhrfpc208dczqfr0hm0y5dc7d
    "28f8ec59c73addbddd2cf4c9068a9cf91188fc31", // bv1q9ruwckw88twmmhfv7nysdz5ulygc3lp3nzslv6
    "622d01e55b69d59532a51ebae61c35dc1d446b08", // bv1qvgksre2md82e2v49r6awv8p4msw5g6cgez5fun
    "4b1296277a5177ae193d2077c507d6e9d9a4a25c", // bv1qfvffvfm629m6uxfaypmu2p7ka8v6fgjuq925km
    "70c0100946f7d1a8e1ca52b34982ecf054feed29", // bv1qwrqpqz2x7lg63cw222e5nqhv7p20amffyskjwv
    "21aa42d64a6e756e9b0c8fd3d5aa7c6f066ab18d", // bv1qyx4y94j2de6kaxcv3lfat2nudurx4vvdaz8ltf
};

inline bool IsPremineActivationBlock(ChainType chain, int height)
{
    return chain == ChainType::MAIN && height == PREMINE_ACTIVATION_HEIGHT;
}

inline std::vector<std::pair<CAmount, CScript>> PremineActivationOutputs()
{
    std::vector<std::pair<CAmount, CScript>> outs;
    outs.reserve(PREMINE_OUTPUT_COUNT);
    for (const char* wp : PREMINE_WITNESS_PROGRAMS) {
        outs.emplace_back(PREMINE_OUTPUT_VALUE, CScript() << OP_0 << ParseHex(wp));
    }
    return outs;
}

inline CAmount PremineActivationTotal()
{
    return PREMINE_OUTPUT_VALUE * PREMINE_OUTPUT_COUNT;
}

/** True if every premine output appears in the coinbase with exact value and script. */
inline bool CoinbaseHasPremineOutputs(const CTransaction& coinbase)
{
    for (const auto& [value, script] : PremineActivationOutputs()) {
        bool found = false;
        for (const CTxOut& out : coinbase.vout) {
            if (out.nValue == value && out.scriptPubKey == script) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

} // namespace bitvault

#endif // BITVAULT_ACTIVATION_H
