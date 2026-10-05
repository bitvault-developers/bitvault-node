// Copyright (c) 2014-2026 The BitVault Core developers
// BIP352 Silent Payments
#include <script/silentpayments.h>
#include <bech32.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <logging.h>
#include <support/cleanse.h>
#include <util/strencodings.h>
#include <cstring>
#include <algorithm>

#include "secp256k1/include/secp256k1.h"
#include "secp256k1/include/secp256k1_ecdh.h"

static secp256k1_context* GetSPCtx() {
    static secp256k1_context* ctx =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return ctx;
}

// BIP340 tagged hash: SHA256(SHA256(tag)||SHA256(tag)||msg)
static uint256 TaggedHash(const std::string& tag, const unsigned char* msg, size_t msg_len)
{
    CSHA256 tag_hasher;
    unsigned char tag_hash[32];
    tag_hasher.Write((const unsigned char*)tag.data(), tag.size()).Finalize(tag_hash);
    CSHA256 h;
    h.Write(tag_hash, 32).Write(tag_hash, 32).Write(msg, msg_len);
    uint256 result;
    h.Finalize(result.begin());
    return result;
}

// ── Address encoding/decoding ──────────────────────────────────────────────

std::string SilentPaymentAddress::ToString() const
{
    std::vector<uint8_t> data;
    // 33 bytes scan + 33 bytes spend = 66 bytes
    const unsigned char* sp = scan_pubkey.begin();
    const unsigned char* ep = spend_pubkey.begin();
    data.insert(data.end(), sp, sp + 33);
    data.insert(data.end(), ep, ep + 33);

    // Convert to 5-bit groups for bech32
    std::vector<uint8_t> conv;
    ConvertBits<8, 5, true>([&](uint8_t v){ conv.push_back(v); }, data.begin(), data.end());
    // Prepend witness version 0
    conv.insert(conv.begin(), 0);
    return bech32::Encode(bech32::Encoding::BECH32M, SILENT_PAYMENT_HRP, conv);
}

std::optional<SilentPaymentAddress> SilentPaymentAddress::FromString(const std::string& addr)
{
    auto dec = bech32::Decode(addr);
    if (dec.encoding != bech32::Encoding::BECH32M) return std::nullopt;
    if (dec.hrp != SILENT_PAYMENT_HRP) return std::nullopt;
    if (dec.data.empty() || dec.data[0] != 0) return std::nullopt;

    std::vector<uint8_t> raw;
    if (!ConvertBits<5, 8, false>([&](uint8_t v){ raw.push_back(v); },
                                   dec.data.begin() + 1, dec.data.end())) {
        return std::nullopt;
    }
    if (raw.size() != 66) return std::nullopt;

    SilentPaymentAddress result;
    result.scan_pubkey  = CPubKey(raw.begin(),      raw.begin() + 33);
    result.spend_pubkey = CPubKey(raw.begin() + 33, raw.end());
    if (!result.IsValid()) return std::nullopt;
    return result;
}

// ── Outpoint hash ──────────────────────────────────────────────────────────

uint256 ComputeOutpointHash(const std::vector<std::pair<uint256, uint32_t>>& outpoints)
{
    // Sort outpoints, then double-SHA256
    auto sorted = outpoints;
    std::sort(sorted.begin(), sorted.end());
    HashWriter hw{};
    for (const auto& [txid, vout] : sorted) {
        hw << txid << vout;
    }
    return hw.GetHash();
}

// ── Sender ─────────────────────────────────────────────────────────────────

std::optional<CPubKey> CreateSilentPaymentOutput(
    const CKey& sender_input_sum_key,
    const SilentPaymentAddress& recipient,
    const uint256& outpoint_hash,
    uint32_t counter)
{
    if (!recipient.IsValid()) return std::nullopt;

    // 1. ECDH: shared = sender_sum_privkey * B_scan
    secp256k1_pubkey scan_pk;
    if (!secp256k1_ec_pubkey_parse(GetSPCtx(), &scan_pk,
            recipient.scan_pubkey.begin(), 33)) return std::nullopt;

    uint8_t ecdh_out[33];
    // Custom hash: return compressed point (x||y prefix)
    if (!secp256k1_ecdh(GetSPCtx(), ecdh_out, &scan_pk,
            sender_input_sum_key.begin(),
            [](unsigned char* out, const unsigned char* x32, const unsigned char* y32, void*) -> int {
                out[0] = (y32[31] & 1) ? 0x03 : 0x02;
                memcpy(out + 1, x32, 32);
                return 1;
            }, nullptr)) return std::nullopt;

    // 2. t_k = int(TaggedHash("BIP352/SharedSecret", ecdh_out || outpoint_hash || counter))
    std::vector<uint8_t> msg(33 + 32 + 4);
    memcpy(msg.data(),      ecdh_out,             33);
    memcpy(msg.data() + 33, outpoint_hash.begin(), 32);
    msg[65] = (counter >> 24) & 0xFF;
    msg[66] = (counter >> 16) & 0xFF;
    msg[67] = (counter >> 8)  & 0xFF;
    msg[68] =  counter        & 0xFF;
    uint256 t = TaggedHash("BIP352/SharedSecret", msg.data(), msg.size());

    // 3. output_pubkey = B_spend + t*G
    secp256k1_pubkey spend_pk;
    if (!secp256k1_ec_pubkey_parse(GetSPCtx(), &spend_pk,
            recipient.spend_pubkey.begin(), 33)) return std::nullopt;

    if (!secp256k1_ec_pubkey_tweak_add(GetSPCtx(), &spend_pk, t.begin()))
        return std::nullopt;

    uint8_t out_bytes[33]; size_t len = 33;
    secp256k1_ec_pubkey_serialize(GetSPCtx(), out_bytes, &len, &spend_pk,
                                  SECP256K1_EC_COMPRESSED);
    CPubKey result(out_bytes, out_bytes + 33);
    return result;
}

// ── Receiver ───────────────────────────────────────────────────────────────
