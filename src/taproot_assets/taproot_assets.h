// taproot_assets.h — Taproot Assets L1 foundation
// Asset issuance, transfer, and commitment structures on BitVault L1
// Drop into: src/taproot_assets/
#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <optional>

namespace BitVault {
namespace TaprootAssets {

using Bytes32  = std::array<uint8_t, 32>;
using PubKey33 = std::array<uint8_t, 33>;

// ── Asset types ────────────────────────────────────────────────────────────
enum class AssetType : uint8_t {
    NORMAL      = 0, // fungible, divisible
    COLLECTIBLE = 1  // non-fungible, indivisible
};

// ── Asset genesis ──────────────────────────────────────────────────────────
struct AssetGenesis {
    Bytes32     genesisPoint;    // outpoint of the first UTXO
    std::string name;
    std::string metaHash;        // H(metadata)
    uint32_t    outputIndex;
    AssetType   type;
    uint64_t    amount;          // initial supply (0 for collectibles)

    Bytes32 id() const;          // H(genesis_point || name || meta || index || type)
};

// ── Asset commitment ───────────────────────────────────────────────────────
struct AssetLeaf {
    Bytes32  assetId;
    uint64_t amount;
    PubKey33 ownerKey;
    Bytes32  prevOutpoint;       // 0 for genesis
    Bytes32  scriptKey;          // tweaked owner key
};

// MS-SMT (Merkle Sum Sparse Merkle Tree) root
struct CommitmentRoot {
    Bytes32  merkleRoot;
    uint64_t totalValue;
};

// Taproot commitment: inner_key = musig2(keys), tapscript = asset_commitment_tree
struct TaprootCommitment {
    PubKey33        outputKey;       // final Taproot output key
    PubKey33        internalKey;     // internal key (MuSig2)
    CommitmentRoot  assetRoot;
    Bytes32         tapLeafHash;     // commitment to asset tree
};

// ── Issuance ───────────────────────────────────────────────────────────────
struct IssuanceRequest {
    std::string      name;
    uint64_t         amount;
    AssetType        type;
    PubKey33         issuerKey;
    std::string      metadata;       // arbitrary JSON
    std::string      tickerSymbol;   // e.g. "USDT", "GOLD"
    uint8_t          decimals;
};

struct IssuedAsset {
    AssetGenesis     genesis;
    TaprootCommitment commitment;
    std::string      txid;          // genesis transaction
    uint32_t         vout;
};

IssuedAsset issueAsset(
    const IssuanceRequest& req,
    const Bytes32& issuancePrivKey,
    const std::string& genesisOutpoint
);

// ── Transfer ───────────────────────────────────────────────────────────────
struct AssetTransfer {
    Bytes32  assetId;
    uint64_t amount;
    PubKey33 recipient;
    Bytes32  prevOutpoint;   // UTXO containing the asset
    PubKey33 prevKey;        // current owner key
};

struct AssetTransferProof {
    AssetTransfer    transfer;
    TaprootCommitment inputCommitment;
    TaprootCommitment outputCommitment;
    std::vector<Bytes32> merkleProof;   // proof of inclusion in asset tree
    Bytes32              signature;     // owner signature
};

AssetTransferProof createTransfer(
    const AssetTransfer& transfer,
    const Bytes32& senderPrivKey
);

bool verifyTransfer(const AssetTransferProof& proof);

// ── Asset script (Tapscript spending conditions) ──────────────────────────
struct AssetScript {
    enum Type { KEY_PATH, HTLC, MULTISIG, VESTING };
    Type                type;
    std::vector<PubKey33> keys;
    uint32_t              threshold;
    uint32_t              locktime;   // for HTLC/vesting
    Bytes32               hashLock;   // for HTLC
};

Bytes32 computeScriptKey(const PubKey33& ownerKey, const AssetScript& script);

// ── Universe (asset registry on L1) ──────────────────────────────────────
struct UniverseRoot {
    Bytes32     root;            // merkle root of all assets
    uint64_t    totalAssets;
    std::string nodeUrl;         // full node URL for asset queries
};

UniverseRoot getUniverseRoot();
bool registerAsset(const IssuedAsset& asset);
std::optional<IssuedAsset> queryAsset(const Bytes32& assetId);

} // namespace TaprootAssets
} // namespace BitVault
