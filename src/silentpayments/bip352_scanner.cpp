// bip352_scanner.cpp — BIP352 Silent Payments implementation
#include "bip352_scanner.h"
#include <util/strencodings.h>
#include <bech32.h>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace BitVault {
namespace SilentPayments {

// ── Tagged hash (BIP340 style) ────────────────────────────────────────────
Bytes32 BIP352Scanner::taggedHash(const std::string& tag, const std::vector<uint8_t>& data) {
    // H_tag(x) = SHA256(SHA256(tag) || SHA256(tag) || x)
    Bytes32 tagHash;
    SHA256(reinterpret_cast<const uint8_t*>(tag.data()), tag.size(), tagHash.data());

    std::vector<uint8_t> preimage;
    preimage.insert(preimage.end(), tagHash.begin(), tagHash.end()); // SHA256(tag)
    preimage.insert(preimage.end(), tagHash.begin(), tagHash.end()); // SHA256(tag) again
    preimage.insert(preimage.end(), data.begin(), data.end());

    Bytes32 result;
    SHA256(preimage.data(), preimage.size(), result.data());
    return result;
}

// ── Input hash: H(outpointLowest || A_sum) ────────────────────────────────
Bytes32 BIP352Scanner::computeInputHash(
    const std::vector<std::string>& outpoints,
    const PubKey33& lowestPubKey
) {
    // Sort outpoints lexicographically, take lowest
    std::vector<std::string> sorted = outpoints;
    std::sort(sorted.begin(), sorted.end());

    std::vector<uint8_t> data;
    // Append lowest outpoint bytes
    const auto& lowest = sorted.front();
    data.insert(data.end(), lowest.begin(), lowest.end());
    // Append sum of input pubkeys (A_sum)
    data.insert(data.end(), lowestPubKey.begin(), lowestPubKey.end());

    return taggedHash("BIP0352/Inputs", data);
}

// ── Tweak: t_k = H(ecdh_secret || ser32(k)) ──────────────────────────────
Bytes32 BIP352Scanner::computeTweak(const Bytes32& ecdhSecret, uint32_t k) {
    std::vector<uint8_t> data(ecdhSecret.begin(), ecdhSecret.end());
    // ser32(k) — 4-byte big-endian
    data.push_back((k >> 24) & 0xFF);
    data.push_back((k >> 16) & 0xFF);
    data.push_back((k >>  8) & 0xFF);
    data.push_back( k        & 0xFF);
    return taggedHash("BIP0352/SharedSecret", data);
}

// ── Derive output key: P_k = B_spend + t_k*G ─────────────────────────────
PubKey33 BIP352Scanner::deriveOutputKey(const PubKey33& spendPubKey, const Bytes32& tweak) {
    PubKey33 result{};
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    EC_POINT* P     = EC_POINT_new(group);
    EC_POINT* T     = EC_POINT_new(group);
    BN_CTX*   ctx   = BN_CTX_new();
    BIGNUM*   t_bn  = BN_bin2bn(tweak.data(), 32, nullptr);

    // Decode B_spend
    EC_POINT_oct2point(group, P, spendPubKey.data(), 33, ctx);

    // T = t_k * G
    EC_POINT_mul(group, T, t_bn, nullptr, nullptr, ctx);

    // P_k = B_spend + T
    EC_POINT* Pk = EC_POINT_new(group);
    EC_POINT_add(group, Pk, P, T, ctx);

    // Encode compressed
    EC_POINT_point2oct(group, Pk, POINT_CONVERSION_COMPRESSED, result.data(), 33, ctx);

    EC_POINT_free(P); EC_POINT_free(T); EC_POINT_free(Pk);
    BN_free(t_bn); BN_CTX_free(ctx); EC_GROUP_free(group);
    return result;
}

BIP352Scanner::BIP352Scanner(const Bytes32& scanPrivKey) : m_scanPrivKey(scanPrivKey) {
    // Derive scan public key
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    EC_POINT* P     = EC_POINT_new(group);
    BN_CTX*   ctx   = BN_CTX_new();
    BIGNUM*   sk    = BN_bin2bn(scanPrivKey.data(), 32, nullptr);

    EC_POINT_mul(group, P, sk, nullptr, nullptr, ctx);
    EC_POINT_point2oct(group, P, POINT_CONVERSION_COMPRESSED, m_scanPubKey.data(), 33, ctx);

    BN_free(sk); BN_CTX_free(ctx); EC_POINT_free(P); EC_GROUP_free(group);
}

// ── ECDH shared secret: a_sum * B_scan ───────────────────────────────────
Bytes32 BIP352Scanner::computeECDHSecret(
    const std::vector<PubKey33>& inputPubKeys,
    const Bytes32& inputHash
) {
    // Sum all input pubkeys: A_sum = A_1 + A_2 + ... + A_n
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx   = BN_CTX_new();
    EC_POINT* Asum  = EC_POINT_new(group);
    EC_POINT_set_to_infinity(group, Asum);

    for (const auto& pk : inputPubKeys) {
        EC_POINT* A = EC_POINT_new(group);
        EC_POINT_oct2point(group, A, pk.data(), 33, ctx);
        EC_POINT_add(group, Asum, Asum, A, ctx);
        EC_POINT_free(A);
    }

    // ECDH: shared = inputHash * A_sum
    BIGNUM*   h    = BN_bin2bn(inputHash.data(), 32, nullptr);
    EC_POINT* shared = EC_POINT_new(group);
    EC_POINT_mul(group, shared, nullptr, Asum, h, ctx);

    // Encode x-coordinate as shared secret
    std::array<uint8_t, 33> enc{};
    EC_POINT_point2oct(group, shared, POINT_CONVERSION_COMPRESSED, enc.data(), 33, ctx);

    Bytes32 result{};
    // Use x-coordinate only (bytes 1-32 of compressed point)
    std::copy(enc.begin() + 1, enc.end(), result.begin());

    BN_free(h); BN_CTX_free(ctx);
    EC_POINT_free(Asum); EC_POINT_free(shared); EC_GROUP_free(group);
    return result;
}

// ── Scan a transaction ────────────────────────────────────────────────────
std::vector<ScannedOutput> BIP352Scanner::scanTx(
    const std::string& txid,
    const std::vector<std::string>& inputOutpoints,
    const std::vector<PubKey33>& inputPubKeys,
    const std::vector<std::pair<std::vector<uint8_t>, uint64_t>>& outputs
) {
    std::vector<ScannedOutput> found;
    if (inputPubKeys.empty() || outputs.empty()) return found;

    // 1. Compute input hash
    PubKey33 lowestPk = *std::min_element(inputPubKeys.begin(), inputPubKeys.end());
    Bytes32  inputHash = computeInputHash(inputOutpoints, lowestPk);

    // 2. ECDH shared secret
    Bytes32  ecdhSecret = computeECDHSecret(inputPubKeys, inputHash);

    // 3. For each output, check if it matches a derived key
    uint32_t k = 0;
    for (size_t i = 0; i < outputs.size(); i++) {
        const auto& [scriptPubKey, value] = outputs[i];
        if (scriptPubKey.size() != 34) continue; // must be P2TR (OP_1 <32-byte>)
        if (scriptPubKey[0] != 0x51) continue;   // OP_1

        // Derive expected output key for index k
        Bytes32  tweak     = computeTweak(ecdhSecret, k);
        PubKey33 outputKey = deriveOutputKey(m_scanPubKey, tweak);

        // Compare x-coordinate (bytes 1-32 of compressed outputKey vs scriptPubKey bytes 2-33)
        bool match = std::equal(outputKey.begin() + 1, outputKey.end(),
                                scriptPubKey.begin() + 2);
        if (match) {
            found.push_back({txid, (uint32_t)i, value, tweak, outputKey, false});
            k++;
        }
    }
    return found;
}

// ── BIP352 Address Encoding (bvsp1... bech32m) ───────────────────────────
std::string BIP352Scanner::encodeAddress(const SilentPaymentAddress& addr) {
    std::vector<uint8_t> payload;
    payload.reserve(66);
    payload.insert(payload.end(), addr.scanPubKey.begin(),  addr.scanPubKey.end());
    payload.insert(payload.end(), addr.spendPubKey.begin(), addr.spendPubKey.end());
    std::vector<uint8_t> data;
    data.push_back(0);
    data.reserve(107);
    if (!ConvertBits<8,5,true>([&](uint8_t b){ data.push_back(b); }, payload.begin(), payload.end()))
        throw std::runtime_error("BIP352: ConvertBits 8->5 failed");
    std::string result = bech32::Encode(bech32::Encoding::BECH32M, "bvsp", data);
    if (result.empty()) throw std::runtime_error("BIP352: bech32m encode failed");
    return result;
}

SilentPaymentAddress BIP352Scanner::decodeAddress(const std::string& encoded) {
    auto dec = bech32::Decode(encoded);
    if (dec.encoding != bech32::Encoding::BECH32M)
        throw std::runtime_error("BIP352: not bech32m: " + encoded);
    if (dec.hrp != "bvsp")
        throw std::runtime_error("BIP352: wrong HRP: " + dec.hrp);
    if (dec.data.empty() || dec.data[0] != 0)
        throw std::runtime_error("BIP352: unknown version");
    std::vector<uint8_t> payload;
    payload.reserve(66);
    if (!ConvertBits<5,8,false>([&](uint8_t b){ payload.push_back(b); }, dec.data.begin()+1, dec.data.end())
        || payload.size() != 66)
        throw std::runtime_error("BIP352: bad payload size: " + std::to_string(payload.size()));
    SilentPaymentAddress addr;
    std::copy(payload.begin(),      payload.begin()+33, addr.scanPubKey.begin());
    std::copy(payload.begin()+33,   payload.end(),      addr.spendPubKey.begin());
    addr.humanReadable = encoded;
    return addr;
}

} // namespace SilentPayments
} // namespace BitVault
