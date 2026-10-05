// bulletproofs.h — Bulletproofs range proof prover for L1
// Proves: value v in [0, 2^n) without revealing v
// Drop into: src/crypto/bulletproofs.h
#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace BitVault {
namespace Bulletproofs {

using Bytes32  = std::array<uint8_t, 32>;
using PubKey33 = std::array<uint8_t, 33>;

// Generator points (deterministically derived)
struct Generators {
    PubKey33 G;  // standard generator
    PubKey33 H;  // H = hash_to_curve("BitVault/BP/H")
    std::vector<PubKey33> Gi; // per-bit generators
    std::vector<PubKey33> Hi;
};

Generators setupGenerators(size_t n); // n = bit range (e.g. 64)

// ── Pedersen commitment: C = v*G + r*H ────────────────────────────────────
struct Commitment {
    PubKey33 point;
    Bytes32  blinding; // r (keep secret)
};

Commitment commit(uint64_t value, const Bytes32& blinding, const Generators& gens);

// ── Range proof ────────────────────────────────────────────────────────────
struct RangeProof {
    PubKey33 A;       // commitment to bits
    PubKey33 S;       // commitment to blinding factors
    PubKey33 T1;      // commitment to t1
    PubKey33 T2;      // commitment to t2
    Bytes32  tau_x;   // blinding for tx
    Bytes32  mu;      // blinding for A, S
    Bytes32  tx;      // inner product value t(x)
    std::vector<PubKey33> L;  // IPA left half
    std::vector<PubKey33> R;  // IPA right half
    Bytes32  a;       // IPA final scalar
    Bytes32  b;       // IPA final scalar
    size_t   n;       // bit range
};

// Prove value is in [0, 2^n)
RangeProof prove(
    uint64_t     value,
    const Bytes32& blinding,
    const Generators& gens,
    size_t       n = 64
);

// Verify a range proof
bool verify(
    const Commitment& commitment,
    const RangeProof& proof,
    const Generators& gens
);

// ── Aggregated proof (m values in one proof — more efficient) ──────────────
struct AggregatedProof {
    std::vector<RangeProof> proofs;
    size_t m; // number of values
};

AggregatedProof proveAggregated(
    const std::vector<uint64_t>&  values,
    const std::vector<Bytes32>&   blindings,
    const Generators& gens,
    size_t n = 64
);

bool verifyAggregated(
    const std::vector<Commitment>& commitments,
    const AggregatedProof& proof,
    const Generators& gens
);

// ── Inner product argument ─────────────────────────────────────────────────
struct IPAProof {
    std::vector<PubKey33> L;
    std::vector<PubKey33> R;
    Bytes32 a;
    Bytes32 b;
};

IPAProof proveIPA(
    const std::vector<Bytes32>& a,
    const std::vector<Bytes32>& b,
    const Generators& gens
);

bool verifyIPA(
    const PubKey33& P, // commitment P = <a,G> + <b,H>
    const Bytes32& c,  // claimed inner product <a,b>
    const IPAProof& proof,
    const Generators& gens
);

// ── Helpers ────────────────────────────────────────────────────────────────
Bytes32  scalarMul(const Bytes32& a, const Bytes32& b);
Bytes32  scalarAdd(const Bytes32& a, const Bytes32& b);
PubKey33 pointAdd(const PubKey33& A, const PubKey33& B);
PubKey33 scalarBaseMul(const Bytes32& s, const PubKey33& G);
Bytes32  fiatShamir(const std::vector<PubKey33>& points, const std::string& label);

} // namespace Bulletproofs
} // namespace BitVault
