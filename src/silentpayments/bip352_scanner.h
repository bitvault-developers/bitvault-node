// bip352_scanner.h — BIP352 Silent Payments UTXO Scanner
// Scans UTXO set for silent payment outputs belonging to a scan key
// Drop into: src/silentpayments/
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <array>

namespace BitVault {
namespace SilentPayments {

// 32-byte key types
using Bytes32  = std::array<uint8_t, 32>;
using PubKey33 = std::array<uint8_t, 33>;

struct SilentPaymentAddress {
    PubKey33 scanPubKey;    // B_scan
    PubKey33 spendPubKey;   // B_spend
    std::string humanReadable; // bvsp1...
};

struct ScannedOutput {
    std::string txid;
    uint32_t    vout;
    uint64_t    value;       // satoshis
    Bytes32     tweak;       // t_k = H(ecdh_secret || k)
    PubKey33    outputKey;   // P_k = B_spend + t_k*G
    bool        claimed;
};

class BIP352Scanner {
public:
    explicit BIP352Scanner(const Bytes32& scanPrivKey);

    // Scan a single transaction for silent payment outputs
    // inputs: serialized tx inputs (outpoints)
    // pubkeys: all input public keys (from scriptSig/witness)
    // outputs: all tx outputs (scriptPubKey, value)
    std::vector<ScannedOutput> scanTx(
        const std::string& txid,
        const std::vector<std::string>& inputOutpoints,
        const std::vector<PubKey33>&    inputPubKeys,
        const std::vector<std::pair<std::vector<uint8_t>, uint64_t>>& outputs
    );

    // Scan entire UTXO set (called at wallet startup / rescan)
    std::vector<ScannedOutput> scanUTXOSet(
        const std::vector<std::string>& txids
    );

    // BIP352 key derivation helpers
    static Bytes32  computeInputHash(
        const std::vector<std::string>& outpoints,
        const PubKey33& lowestPubKey
    );
    static PubKey33 deriveOutputKey(
        const PubKey33& spendPubKey,
        const Bytes32&  tweak
    );
    static Bytes32  computeTweak(
        const Bytes32& ecdhSecret,
        uint32_t       outputIndex
    );

    // Encode/decode BitVault silent payment address (bvsp1...)
    static std::string encodeAddress(const SilentPaymentAddress& addr);
    static SilentPaymentAddress decodeAddress(const std::string& encoded);

private:
    Bytes32  m_scanPrivKey;
    PubKey33 m_scanPubKey;

    // ECDH shared secret: a_sum * B_scan
    Bytes32 computeECDHSecret(
        const std::vector<PubKey33>& inputPubKeys,
        const Bytes32& inputHash
    );

    // Tagged hash: H_tag(data)
    static Bytes32 taggedHash(const std::string& tag, const std::vector<uint8_t>& data);
};

} // namespace SilentPayments
} // namespace BitVault
