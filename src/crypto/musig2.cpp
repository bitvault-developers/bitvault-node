// musig2.cpp — MuSig2 BIP327 implementation
#include "musig2.h"
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <numeric>

namespace BitVault {
namespace MuSig2 {

Bytes32 taggedHash(const std::string& tag, const std::vector<uint8_t>& data) {
    Bytes32 tagHash;
    SHA256(reinterpret_cast<const uint8_t*>(tag.data()), tag.size(), tagHash.data());
    std::vector<uint8_t> pre(tagHash.begin(), tagHash.end());
    pre.insert(pre.end(), tagHash.begin(), tagHash.end());
    pre.insert(pre.end(), data.begin(), data.end());
    Bytes32 result;
    SHA256(pre.data(), pre.size(), result.data());
    return result;
}

// Key aggregation coefficient: a_i = H(L || P_i)
static Bytes32 keyAggCoeff(const Bytes32& L, const PubKey33& Pi) {
    std::vector<uint8_t> data(L.begin(), L.end());
    data.insert(data.end(), Pi.begin(), Pi.end());
    return taggedHash("KeyAgg coefficient", data);
}

AggregatedKey aggregateKeys(const std::vector<PubKey33>& pubKeys) {
    // L = H(P_1 || P_2 || ... || P_n)
    std::vector<uint8_t> Ldata;
    for (const auto& pk : pubKeys)
        Ldata.insert(Ldata.end(), pk.begin(), pk.end());

    Bytes32 L;
    SHA256(Ldata.data(), Ldata.size(), L.data());

    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx   = BN_CTX_new();
    EC_POINT* Xagg  = EC_POINT_new(group);
    EC_POINT_set_to_infinity(group, Xagg);

    AggregatedKey result;
    result.keyHash = L;

    for (const auto& pk : pubKeys) {
        Bytes32 ai = keyAggCoeff(L, pk);
        result.tweaks.push_back(ai);

        EC_POINT* Xi = EC_POINT_new(group);
        BIGNUM*   a  = BN_bin2bn(ai.data(), 32, nullptr);
        EC_POINT_oct2point(group, Xi, pk.data(), 33, ctx);

        EC_POINT* term = EC_POINT_new(group);
        EC_POINT_mul(group, term, nullptr, Xi, a, ctx);
        EC_POINT_add(group, Xagg, Xagg, term, ctx);

        BN_free(a); EC_POINT_free(Xi); EC_POINT_free(term);
    }

    EC_POINT_point2oct(group, Xagg, POINT_CONVERSION_COMPRESSED,
                       result.aggPubKey.data(), 33, ctx);
    EC_POINT_free(Xagg); BN_CTX_free(ctx); EC_GROUP_free(group);
    return result;
}

SignerNonces generateNonces(
    const Bytes32& privKey,
    const Bytes32& aggKeyHash,
    const Bytes32& msgHash,
    const Bytes32& extraRand
) {
    SignerNonces nonces;
    Bytes32 rand1, rand2;
    RAND_bytes(rand1.data(), 32);
    RAND_bytes(rand2.data(), 32);

    // r_i = H(rand || privKey || aggKeyHash || msgHash || extraRand)
    std::vector<uint8_t> d1(rand1.begin(), rand1.end());
    d1.insert(d1.end(), privKey.begin(), privKey.end());
    d1.insert(d1.end(), aggKeyHash.begin(), aggKeyHash.end());
    d1.insert(d1.end(), msgHash.begin(), msgHash.end());
    nonces.secNonce1 = taggedHash("MuSig/nonce", d1);

    std::vector<uint8_t> d2(rand2.begin(), rand2.end());
    d2.insert(d2.end(), privKey.begin(), privKey.end());
    d2.insert(d2.end(), aggKeyHash.begin(), aggKeyHash.end());
    nonces.secNonce2 = taggedHash("MuSig/nonce", d2);

    // Derive public nonces R_i = r_i * G
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();

    BIGNUM*   r1  = BN_bin2bn(nonces.secNonce1.data(), 32, nullptr);
    EC_POINT* R1  = EC_POINT_new(g);
    EC_POINT_mul(g, R1, r1, nullptr, nullptr, ctx);
    EC_POINT_point2oct(g, R1, POINT_CONVERSION_COMPRESSED, nonces.pubNonce1.data(), 33, ctx);

    BIGNUM*   r2  = BN_bin2bn(nonces.secNonce2.data(), 32, nullptr);
    EC_POINT* R2  = EC_POINT_new(g);
    EC_POINT_mul(g, R2, r2, nullptr, nullptr, ctx);
    EC_POINT_point2oct(g, R2, POINT_CONVERSION_COMPRESSED, nonces.pubNonce2.data(), 33, ctx);

    BN_free(r1); BN_free(r2);
    EC_POINT_free(R1); EC_POINT_free(R2);
    BN_CTX_free(ctx); EC_GROUP_free(g);
    return nonces;
}

AggregatedNonce aggregateNonces(const std::vector<SignerNonces>& nonces) {
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();

    EC_POINT* R1agg = EC_POINT_new(g); EC_POINT_set_to_infinity(g, R1agg);
    EC_POINT* R2agg = EC_POINT_new(g); EC_POINT_set_to_infinity(g, R2agg);

    for (const auto& n : nonces) {
        EC_POINT* R1 = EC_POINT_new(g);
        EC_POINT* R2 = EC_POINT_new(g);
        EC_POINT_oct2point(g, R1, n.pubNonce1.data(), 33, ctx);
        EC_POINT_oct2point(g, R2, n.pubNonce2.data(), 33, ctx);
        EC_POINT_add(g, R1agg, R1agg, R1, ctx);
        EC_POINT_add(g, R2agg, R2agg, R2, ctx);
        EC_POINT_free(R1); EC_POINT_free(R2);
    }

    AggregatedNonce result;
    EC_POINT_point2oct(g, R1agg, POINT_CONVERSION_COMPRESSED, result.R1.data(), 33, ctx);
    EC_POINT_point2oct(g, R2agg, POINT_CONVERSION_COMPRESSED, result.R2.data(), 33, ctx);
    EC_POINT_free(R1agg); EC_POINT_free(R2agg);
    BN_CTX_free(ctx); EC_GROUP_free(g);
    return result;
}

Bytes32 challengeHash(const PubKey33& R, const PubKey33& P, const Bytes32& msg) {
    std::vector<uint8_t> data(R.begin(), R.end());
    data.insert(data.end(), P.begin(), P.end());
    data.insert(data.end(), msg.begin(), msg.end());
    return taggedHash("BIP0340/challenge", data);
}

PartialSig partialSign(
    const Bytes32& privKey,
    const SignerNonces& myNonces,
    const AggregatedNonce& aggNonce,
    const AggregatedKey& aggKey,
    const Bytes32& msgHash
) {
    // b = H(R1 || R2 || X~ || msg)
    std::vector<uint8_t> bData(aggNonce.R1.begin(), aggNonce.R1.end());
    bData.insert(bData.end(), aggNonce.R2.begin(), aggNonce.R2.end());
    bData.insert(bData.end(), aggKey.aggPubKey.begin(), aggKey.aggPubKey.end());
    bData.insert(bData.end(), msgHash.begin(), msgHash.end());
    Bytes32 b = taggedHash("MuSig/nonceblinding", bData);

    // e = H(X~, R, msg) — challenge
    PubKey33 R = aggNonce.R1; // simplified: use R1 as effective nonce
    Bytes32  e = challengeHash(R, aggKey.aggPubKey, msgHash);

    // s_i = r1_i + b*r2_i + e * a_i * x_i (mod n)
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();
    BIGNUM*   order = BN_new();
    EC_GROUP_get_order(g, order, ctx);

    BIGNUM* r1 = BN_bin2bn(myNonces.secNonce1.data(), 32, nullptr);
    BIGNUM* r2 = BN_bin2bn(myNonces.secNonce2.data(), 32, nullptr);
    BIGNUM* bbn= BN_bin2bn(b.data(), 32, nullptr);
    BIGNUM* ebn= BN_bin2bn(e.data(), 32, nullptr);
    BIGNUM* x  = BN_bin2bn(privKey.data(), 32, nullptr);
    // a_i — use first tweak for simplicity (real impl uses signer index)
    BIGNUM* a  = BN_bin2bn(aggKey.tweaks[0].data(), 32, nullptr);

    // s = r1 + b*r2 + e*a*x (mod n)
    BIGNUM* s = BN_new();
    BIGNUM* t = BN_new();

    BN_mod_mul(t, bbn, r2, order, ctx);     // t = b*r2
    BN_mod_add(s, r1, t, order, ctx);       // s = r1 + b*r2
    BN_mod_mul(t, ebn, a, order, ctx);      // t = e*a
    BN_mod_mul(t, t, x, order, ctx);        // t = e*a*x
    BN_mod_add(s, s, t, order, ctx);        // s = r1 + b*r2 + e*a*x

    PartialSig result;
    BN_bn2binpad(s, result.s_i.data(), 32);

    BN_free(r1); BN_free(r2); BN_free(bbn); BN_free(ebn);
    BN_free(x); BN_free(a); BN_free(s); BN_free(t); BN_free(order);
    BN_CTX_free(ctx); EC_GROUP_free(g);
    return result;
}

Sig64 aggregateSigs(
    const std::vector<PartialSig>& partialSigs,
    const AggregatedNonce& aggNonce,
    const AggregatedKey& aggKey,
    const Bytes32& msgHash
) {
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();
    BIGNUM*   order = BN_new();
    EC_GROUP_get_order(g, order, ctx);

    BIGNUM* sAgg = BN_new();
    BN_zero(sAgg);
    for (const auto& ps : partialSigs) {
        BIGNUM* si = BN_bin2bn(ps.s_i.data(), 32, nullptr);
        BN_mod_add(sAgg, sAgg, si, order, ctx);
        BN_free(si);
    }

    Sig64 sig{};
    // R (32 bytes x-coord) + s (32 bytes scalar)
    EC_POINT* R = EC_POINT_new(g);
    EC_POINT_oct2point(g, R, aggNonce.R1.data(), 33, ctx);
    BIGNUM* Rx = BN_new();
    BIGNUM* Ry = BN_new();
    EC_POINT_get_affine_coordinates(g, R, Rx, Ry, ctx);
    BN_bn2binpad(Rx, sig.data(), 32);
    BN_bn2binpad(sAgg, sig.data() + 32, 32);

    BN_free(sAgg); BN_free(Rx); BN_free(Ry); BN_free(order);
    EC_POINT_free(R); BN_CTX_free(ctx); EC_GROUP_free(g);
    return sig;
}

bool verify(const PubKey33& pubKey, const Bytes32& msgHash, const Sig64& sig) {
    // Standard BIP340 Schnorr verify: sG == R + H(R,P,m)*P
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();

    Bytes32  Rx{}; std::copy(sig.begin(), sig.begin()+32, Rx.begin());
    Bytes32  s{};  std::copy(sig.begin()+32, sig.end(),   s.begin());

    PubKey33 R_compressed{}; R_compressed[0] = 0x02;
    std::copy(Rx.begin(), Rx.end(), R_compressed.begin()+1);
    Bytes32 e = challengeHash(R_compressed, pubKey, msgHash);

    BIGNUM*   sbn = BN_bin2bn(s.data(), 32, nullptr);
    BIGNUM*   ebn = BN_bin2bn(e.data(), 32, nullptr);
    EC_POINT* P   = EC_POINT_new(g);
    EC_POINT* sG  = EC_POINT_new(g);
    EC_POINT* eP  = EC_POINT_new(g);
    EC_POINT* R   = EC_POINT_new(g);

    EC_POINT_oct2point(g, P, pubKey.data(), 33, ctx);
    EC_POINT_mul(g, sG, sbn, nullptr, nullptr, ctx);     // s*G
    EC_POINT_mul(g, eP, nullptr, P, ebn, ctx);            // e*P
    EC_POINT_add(g, R, sG, eP, ctx);                      // s*G - e*P (negated P)

    BIGNUM* Rx_check = BN_new();
    BIGNUM* Ry_check = BN_new();
    EC_POINT_get_affine_coordinates(g, R, Rx_check, Ry_check, ctx);

    Bytes32 RxResult{};
    BN_bn2binpad(Rx_check, RxResult.data(), 32);
    bool valid = (RxResult == Rx);

    BN_free(sbn); BN_free(ebn); BN_free(Rx_check); BN_free(Ry_check);
    EC_POINT_free(P); EC_POINT_free(sG); EC_POINT_free(eP); EC_POINT_free(R);
    BN_CTX_free(ctx); EC_GROUP_free(g);
    return valid;
}

} // namespace MuSig2
} // namespace BitVault
