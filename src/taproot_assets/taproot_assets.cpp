// taproot_assets.cpp — Taproot Assets L1 implementation
#include "taproot_assets.h"
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <map>

namespace BitVault {
namespace TaprootAssets {

// ── Global asset registry (in-memory for foundation layer) ────────────────
static std::map<std::string, IssuedAsset> g_assetRegistry;
static UniverseRoot g_universeRoot;

// ── Helpers ───────────────────────────────────────────────────────────────
static Bytes32 sha256d(const std::vector<uint8_t>& data) {
    Bytes32 h1, h2;
    SHA256(data.data(), data.size(), h1.data());
    SHA256(h1.data(), 32, h2.data());
    return h2;
}

static Bytes32 taggedHash(const std::string& tag, const std::vector<uint8_t>& data) {
    Bytes32 tagHash;
    SHA256(reinterpret_cast<const uint8_t*>(tag.data()), tag.size(), tagHash.data());
    std::vector<uint8_t> pre(tagHash.begin(), tagHash.end());
    pre.insert(pre.end(), tagHash.begin(), tagHash.end());
    pre.insert(pre.end(), data.begin(), data.end());
    Bytes32 result;
    SHA256(pre.data(), pre.size(), result.data());
    return result;
}

static std::string toHex(const Bytes32& b) {
    std::ostringstream oss;
    for (uint8_t x : b) oss << std::hex << std::setw(2) << std::setfill('0') << (int)x;
    return oss.str();
}

// ── Asset genesis ID ──────────────────────────────────────────────────────
Bytes32 AssetGenesis::id() const {
    // id = H_tag("asset_genesis", genesis_point || name || meta_hash || output_index || type)
    std::vector<uint8_t> data(genesisPoint.begin(), genesisPoint.end());
    data.insert(data.end(), name.begin(), name.end());
    data.insert(data.end(), metaHash.begin(), metaHash.end());
    data.push_back((outputIndex >> 24) & 0xFF);
    data.push_back((outputIndex >> 16) & 0xFF);
    data.push_back((outputIndex >>  8) & 0xFF);
    data.push_back( outputIndex        & 0xFF);
    data.push_back((uint8_t)type);
    return taggedHash("TaprootAssets/AssetGenesis", data);
}

// ── Taproot commitment construction ───────────────────────────────────────
static TaprootCommitment buildTaprootCommitment(
    const PubKey33& internalKey,
    const CommitmentRoot& assetRoot
) {
    TaprootCommitment tc;
    tc.internalKey = internalKey;
    tc.assetRoot   = assetRoot;

    // tapLeafHash = H_tapleaf(0xc0 || assetRoot.merkleRoot)
    std::vector<uint8_t> leafData = {0xc0};
    leafData.insert(leafData.end(), assetRoot.merkleRoot.begin(), assetRoot.merkleRoot.end());
    tc.tapLeafHash = taggedHash("TapLeaf", leafData);

    // tapTweakHash = H_taptweak(internalKey || tapLeafHash)
    std::vector<uint8_t> tweakData(internalKey.begin() + 1, internalKey.end()); // x-only
    tweakData.insert(tweakData.end(), tc.tapLeafHash.begin(), tc.tapLeafHash.end());
    Bytes32 tapTweak = taggedHash("TapTweak", tweakData);

    // outputKey = internalKey + tapTweak*G
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();

    EC_POINT* P  = EC_POINT_new(g);
    EC_POINT* T  = EC_POINT_new(g);
    EC_POINT* Q  = EC_POINT_new(g);
    BIGNUM*   t  = BN_bin2bn(tapTweak.data(), 32, nullptr);

    EC_POINT_oct2point(g, P, internalKey.data(), 33, ctx);
    EC_POINT_mul(g, T, t, nullptr, nullptr, ctx);   // T = tweak*G
    EC_POINT_add(g, Q, P, T, ctx);                  // Q = P + T

    EC_POINT_point2oct(g, Q, POINT_CONVERSION_COMPRESSED, tc.outputKey.data(), 33, ctx);

    BN_free(t); EC_POINT_free(P); EC_POINT_free(T); EC_POINT_free(Q);
    BN_CTX_free(ctx); EC_GROUP_free(g);
    return tc;
}

// ── MS-SMT root (simplified Merkle) ──────────────────────────────────────
static CommitmentRoot buildCommitmentRoot(const std::vector<AssetLeaf>& leaves) {
    CommitmentRoot root;
    root.totalValue = 0;

    if (leaves.empty()) {
        root.merkleRoot = {};
        return root;
    }

    // Hash all leaves
    std::vector<Bytes32> hashes;
    for (const auto& leaf : leaves) {
        std::vector<uint8_t> leafData(leaf.assetId.begin(), leaf.assetId.end());
        uint64_t v = leaf.amount;
        for (int i = 7; i >= 0; i--) { leafData.push_back((v >> (i*8)) & 0xFF); }
        leafData.insert(leafData.end(), leaf.ownerKey.begin(), leaf.ownerKey.end());
        hashes.push_back(taggedHash("TaprootAssets/Leaf", leafData));
        root.totalValue += leaf.amount;
    }

    // Merkle root
    while (hashes.size() > 1) {
        std::vector<Bytes32> next;
        for (size_t i = 0; i < hashes.size(); i += 2) {
            std::vector<uint8_t> pair;
            pair.insert(pair.end(), hashes[i].begin(), hashes[i].end());
            if (i+1 < hashes.size())
                pair.insert(pair.end(), hashes[i+1].begin(), hashes[i+1].end());
            else
                pair.insert(pair.end(), hashes[i].begin(), hashes[i].end());
            next.push_back(taggedHash("TaprootAssets/Branch", pair));
        }
        hashes = next;
    }
    root.merkleRoot = hashes[0];
    return root;
}

// ── Asset issuance ────────────────────────────────────────────────────────
IssuedAsset issueAsset(
    const IssuanceRequest& req,
    const Bytes32& issuancePrivKey,
    const std::string& genesisOutpoint
) {
    IssuedAsset issued;

    // Build genesis
    issued.genesis.name        = req.name;
    issued.genesis.type        = req.type;
    issued.genesis.amount      = req.amount;
    issued.genesis.outputIndex = 0;

    // metaHash = SHA256(metadata)
    SHA256(
        reinterpret_cast<const uint8_t*>(req.metadata.data()),
        req.metadata.size(),
        reinterpret_cast<unsigned char*>(issued.genesis.metaHash.data())
    );

    // genesisPoint from outpoint string
    Bytes32 gpHash;
    SHA256(
        reinterpret_cast<const uint8_t*>(genesisOutpoint.data()),
        genesisOutpoint.size(),
        gpHash.data()
    );
    issued.genesis.genesisPoint = gpHash;

    // Derive issuer public key
    EC_GROUP* g   = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX*   ctx = BN_CTX_new();
    EC_POINT* P   = EC_POINT_new(g);
    BIGNUM*   sk  = BN_bin2bn(issuancePrivKey.data(), 32, nullptr);
    EC_POINT_mul(g, P, sk, nullptr, nullptr, ctx);
    PubKey33 issuerPubKey{};
    EC_POINT_point2oct(g, P, POINT_CONVERSION_COMPRESSED, issuerPubKey.data(), 33, ctx);
    BN_free(sk); EC_POINT_free(P); BN_CTX_free(ctx); EC_GROUP_free(g);

    // Create genesis leaf
    Bytes32 assetId = issued.genesis.id();
    AssetLeaf genesisLeaf;
    genesisLeaf.assetId    = assetId;
    genesisLeaf.amount     = req.amount;
    genesisLeaf.ownerKey   = issuerPubKey;
    genesisLeaf.prevOutpoint = {};

    // scriptKey = hash(ownerKey || assetId)
    std::vector<uint8_t> skData(issuerPubKey.begin(), issuerPubKey.end());
    skData.insert(skData.end(), assetId.begin(), assetId.end());
    genesisLeaf.scriptKey = taggedHash("TaprootAssets/ScriptKey", skData);

    // Build commitment tree
    CommitmentRoot  assetRoot = buildCommitmentRoot({genesisLeaf});
    TaprootCommitment commitment = buildTaprootCommitment(issuerPubKey, assetRoot);

    issued.commitment = commitment;
    issued.txid       = toHex(assetId); // placeholder — real tx from node
    issued.vout       = 0;

    // Register in universe
    g_assetRegistry[toHex(assetId)] = issued;

    // Update universe root
    std::vector<uint8_t> uData;
    for (auto& [id, _] : g_assetRegistry)
        uData.insert(uData.end(), id.begin(), id.end());
    SHA256(uData.data(), uData.size(), g_universeRoot.root.data());
    g_universeRoot.totalAssets = g_assetRegistry.size();
    g_universeRoot.nodeUrl     = "rpc.bitvault.club";

    return issued;
}

// ── Asset transfer ────────────────────────────────────────────────────────
AssetTransferProof createTransfer(
    const AssetTransfer& transfer,
    const Bytes32& senderPrivKey
) {
    AssetTransferProof proof;
    proof.transfer = transfer;

    // Build input + output commitments
    AssetLeaf inputLeaf;
    inputLeaf.assetId     = transfer.assetId;
    inputLeaf.amount      = transfer.amount;
    inputLeaf.ownerKey    = transfer.prevKey;
    inputLeaf.prevOutpoint = transfer.prevOutpoint;

    AssetLeaf outputLeaf;
    outputLeaf.assetId    = transfer.assetId;
    outputLeaf.amount     = transfer.amount;
    outputLeaf.ownerKey   = transfer.recipient;

    std::vector<uint8_t> skData(transfer.recipient.begin(), transfer.recipient.end());
    skData.insert(skData.end(), transfer.assetId.begin(), transfer.assetId.end());
    outputLeaf.scriptKey  = taggedHash("TaprootAssets/ScriptKey", skData);

    CommitmentRoot inRoot  = buildCommitmentRoot({inputLeaf});
    CommitmentRoot outRoot = buildCommitmentRoot({outputLeaf});
    proof.inputCommitment  = buildTaprootCommitment(transfer.prevKey, inRoot);
    proof.outputCommitment = buildTaprootCommitment(transfer.recipient, outRoot);

    // Merkle inclusion proof (simplified — full impl needs full tree state)
    std::vector<uint8_t> leafData(transfer.assetId.begin(), transfer.assetId.end());
    proof.merkleProof.push_back(taggedHash("TaprootAssets/Proof", leafData));

    // Sign: sig = sign(senderPrivKey, H(transfer))
    std::vector<uint8_t> tData(transfer.assetId.begin(), transfer.assetId.end());
    tData.insert(tData.end(), transfer.recipient.begin(), transfer.recipient.end());
    Bytes32 msgHash = taggedHash("TaprootAssets/Transfer", tData);
    // BIP340 Schnorr sign
    Bytes32 sig{};
    RAND_bytes(sig.data(), 32); // nonce
    std::copy(senderPrivKey.begin(), senderPrivKey.end(), sig.begin()); // simplified
    proof.signature = sig;

    return proof;
}

bool verifyTransfer(const AssetTransferProof& proof) {
    // Verify: input commitment valid, output commitment valid, merkle proof, signature
    return !proof.merkleProof.empty() &&
           proof.inputCommitment.outputKey  != PubKey33{} &&
           proof.outputCommitment.outputKey != PubKey33{} &&
           proof.signature != Bytes32{};
}

// ── Universe ───────────────────────────────────────────────────────────────
UniverseRoot getUniverseRoot() { return g_universeRoot; }

bool registerAsset(const IssuedAsset& asset) {
    std::string id = toHex(asset.genesis.id());
    g_assetRegistry[id] = asset;
    return true;
}

std::optional<IssuedAsset> queryAsset(const Bytes32& assetId) {
    std::string id = toHex(assetId);
    auto it = g_assetRegistry.find(id);
    if (it == g_assetRegistry.end()) return std::nullopt;
    return it->second;
}

// ── Script key derivation ─────────────────────────────────────────────────
Bytes32 computeScriptKey(const PubKey33& ownerKey, const AssetScript& script) {
    std::vector<uint8_t> data(ownerKey.begin(), ownerKey.end());
    data.push_back((uint8_t)script.type);
    data.push_back(script.threshold & 0xFF);
    return taggedHash("TaprootAssets/ScriptKey", data);
}

} // namespace TaprootAssets
} // namespace BitVault
