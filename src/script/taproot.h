#ifndef BITVAULT_SCRIPT_TAPROOT_H
#define BITVAULT_SCRIPT_TAPROOT_H
#include <crypto/schnorr.h>
#include <script/script.h>
#include <uint256.h>
#include <array>
#include <optional>
#include <vector>
static constexpr uint8_t TAPROOT_LEAF_TAPSCRIPT = 0xc0;
static constexpr uint8_t TAPROOT_CONTROL_BASE_SIZE = 33;
static constexpr uint8_t TAPROOT_CONTROL_NODE_SIZE = 32;
static constexpr size_t  TAPROOT_CONTROL_MAX_NODE_COUNT = 128;
static constexpr size_t  TAPROOT_CONTROL_MAX_SIZE = TAPROOT_CONTROL_BASE_SIZE + TAPROOT_CONTROL_NODE_SIZE * TAPROOT_CONTROL_MAX_NODE_COUNT;
class XOnlyPubKey {
public:
    static constexpr size_t SIZE = 32;
    XOnlyPubKey() = default;
    explicit XOnlyPubKey(const uint8_t* bytes) { memcpy(m_keydata.data(), bytes, SIZE); }
    bool IsValid() const;
    bool VerifySchnorr(const uint256& hash, const SchnorrSig& sig) const;
    std::optional<XOnlyPubKey> ComputeTaprootOutputKey(const uint256* merkle_root=nullptr, bool* parity_out=nullptr) const;
    const uint8_t* data() const { return m_keydata.data(); }
    uint8_t*       data()       { return m_keydata.data(); }
    bool operator==(const XOnlyPubKey& o) const { return m_keydata==o.m_keydata; }
    bool operator<(const XOnlyPubKey& o)  const { return m_keydata< o.m_keydata; }
private:
    std::array<uint8_t,SIZE> m_keydata{};
};
struct TapLeaf { uint8_t leaf_version=TAPROOT_LEAF_TAPSCRIPT; CScript script; uint256 Hash() const; };
uint256 ComputeTapBranchHash(const uint256& a, const uint256& b);
uint256 ComputeTapTreeRoot(const std::vector<TapLeaf>& leaves);
std::optional<XOnlyPubKey> ComputeTaprootOutputKey(const XOnlyPubKey& internal_key, const uint256& merkle_root, bool* parity_out=nullptr);
bool VerifyTaprootCommitment(const std::vector<uint8_t>& control, const std::vector<uint8_t>& program, const uint256& tapleaf_hash);
CScript GetP2TRScript(const XOnlyPubKey& output_key);
bool IsP2TR(const CScript& script);
bool GetP2TRKey(const CScript& script, XOnlyPubKey& key_out);
#endif
