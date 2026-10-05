// musig2.h — MuSig2 BIP327 Schnorr signature aggregation for L1
// Multi-party signing: n-of-n threshold aggregated Schnorr signature
// Drop into: src/crypto/musig2.h
#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <optional>

namespace BitVault {
namespace MuSig2 {

using Bytes32  = std::array<uint8_t, 32>;
using PubKey33 = std::array<uint8_t, 33>;
using Sig64    = std::array<uint8_t, 64>;

// ── Key aggregation ───────────────────────────────────────────────────────
struct AggregatedKey {
    PubKey33 aggPubKey;     // X~ = H(L) * X_1 + ... + H(L) * X_n
    Bytes32  keyHash;       // H(L) = H(P_1 || P_2 || ... || P_n)
    std::vector<Bytes32> tweaks; // per-signer tweaks a_i = H(L || P_i)
};

AggregatedKey aggregateKeys(const std::vector<PubKey33>& pubKeys);

// ── Nonce generation (phase 1) ────────────────────────────────────────────
struct SignerNonces {
    Bytes32 secNonce1;  // secret r_1
    Bytes32 secNonce2;  // secret r_2
    PubKey33 pubNonce1; // R_1 = r_1 * G
    PubKey33 pubNonce2; // R_2 = r_2 * G
};

SignerNonces generateNonces(
    const Bytes32& privKey,
    const Bytes32& aggKeyHash,
    const Bytes32& msgHash,
    const Bytes32& extraRand  // additional entropy
);

// ── Nonce aggregation (coordinator) ──────────────────────────────────────
struct AggregatedNonce {
    PubKey33 R1; // sum of all R_1_i
    PubKey33 R2; // sum of all R_2_i
};

AggregatedNonce aggregateNonces(const std::vector<SignerNonces>& nonces);

// ── Partial signing (each signer) ────────────────────────────────────────
struct PartialSig {
    Bytes32 s_i;  // s_i = r_1_i + b*r_2_i + H(X~, R, m) * a_i * x_i
};

PartialSig partialSign(
    const Bytes32&         privKey,
    const SignerNonces&    myNonces,
    const AggregatedNonce& aggNonce,
    const AggregatedKey&   aggKey,
    const Bytes32&         msgHash
);

// ── Signature aggregation (coordinator) ──────────────────────────────────
Sig64 aggregateSigs(
    const std::vector<PartialSig>& partialSigs,
    const AggregatedNonce&         aggNonce,
    const AggregatedKey&           aggKey,
    const Bytes32&                 msgHash
);

// ── Verify aggregated Schnorr signature (BIP340 compatible) ───────────────
bool verify(
    const PubKey33& aggPubKey,
    const Bytes32&  msgHash,
    const Sig64&    sig
);

// ── Partial sig verification (each signer's contribution) ─────────────────
bool verifyPartial(
    const PubKey33&        signerPubKey,
    const PartialSig&      partialSig,
    const AggregatedNonce& aggNonce,
    const AggregatedKey&   aggKey,
    const Bytes32&         msgHash,
    size_t                 signerIndex
);

// ── Helpers ───────────────────────────────────────────────────────────────
Bytes32  taggedHash(const std::string& tag, const std::vector<uint8_t>& data);
Bytes32  challengeHash(const PubKey33& R, const PubKey33& P, const Bytes32& msg);

} // namespace MuSig2
} // namespace BitVault
