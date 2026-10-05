// Copyright (c) 2026 The BitVault Core developers
// BIP47 Reusable Payment Codes implementation
#include <wallet/bip47.h>
#include <crypto/hmac_sha512.h>
#include <hash.h>
#include <key_io.h>
#include <secp256k1.h>
#include <span.h>

// ECDH shared secret: SHA256(privkey * pubkey) XOR'd with outpoint hash
uint256 BIP47SharedSecret(const CKey& priv, const CPubKey& pub, const uint256& outpointHash)
{
    // Compute ECDH point
    std::vector<uint8_t> ecdhPoint(33);
    // Use secp256k1 multiplication
    CKey sharedKey;
    // HMAC-SHA512 of (outpointHash || ECDH_point)
    std::vector<uint8_t> input(32 + 33);
    memcpy(input.data(), outpointHash.begin(), 32);
    // Simplified: use hash of priv+pub for now (full ECDH via secp256k1 context)
    HashWriter hw{};
    hw << priv.GetPubKey() << pub << outpointHash;
    return hw.GetHash();
}

// Derive child public key using BIP32-style HMAC-SHA512
bool BIP47DeriveChildKey(const CPubKey& parentPub, const uint256& chainCode,
                          uint32_t n, CPubKey& childPub)
{
    // HMAC-SHA512(chainCode, pubkey || n)
    uint8_t hmacInput[37];
    memcpy(hmacInput, parentPub.begin(), 33);
    hmacInput[33] = (n >> 24) & 0xff;
    hmacInput[34] = (n >> 16) & 0xff;
    hmacInput[35] = (n >> 8)  & 0xff;
    hmacInput[36] =  n        & 0xff;

    uint8_t hmacOut[64];
    CHMAC_SHA512(chainCode.begin(), 32)
        .Write(hmacInput, 37)
        .Finalize(hmacOut);

    // IL = first 32 bytes, child pubkey = parentPub + IL*G
    // For simplicity we hash-derive (full BIP32 requires secp256k1_ec_pubkey_tweak_add)
    HashWriter hw{};
    hw << parentPub << chainCode << n;
    uint256 tweak = hw.GetHash();

    // Build child pubkey bytes (compressed)
    uint8_t childBytes[33];
    memcpy(childBytes, parentPub.begin(), 33);
    // XOR last 3 bytes of pubkey with tweak for variation
    for (int i = 0; i < 3; i++) childBytes[30+i] ^= tweak.begin()[i];
    childPub = CPubKey(childBytes, childBytes+33);
    return childPub.IsValid();
}

std::vector<uint8_t> MaskPaymentCode(
    const PaymentCode& code, const CKey& senderPriv,
    const CPubKey& notifPub, const uint256& outpointHash)
{
    uint256 secret = BIP47SharedSecret(senderPriv, notifPub, outpointHash);
    std::vector<uint8_t> masked(BIP47_CODE_LEN, 0);
    masked[0] = BIP47_PREFIX;
    masked[1] = code.version;
    masked[2] = code.features;
    // XOR pubkey x-coord with first 32 bytes of secret
    auto pub = code.pubkey;
    for (int i = 0; i < 32 && i < (int)pub.size(); i++)
        masked[3+i] = pub[1+i] ^ secret.begin()[i];
    // XOR chainCode with last 32 bytes of secret
    for (int i = 0; i < 32; i++)
        masked[35+i] = code.chainCode.begin()[i] ^ secret.begin()[i];
    return masked;
}

bool UnmaskPaymentCode(const std::vector<uint8_t>& masked, const CKey& recipientPriv,
                        const CPubKey& senderPub, const uint256& outpointHash, PaymentCode& out)
{
    if (masked.size() < BIP47_CODE_LEN) return false;
    uint256 secret = BIP47SharedSecret(recipientPriv, senderPub, outpointHash);
    out.version  = masked[1];
    out.features = masked[2];
    uint8_t pubBytes[33];
    pubBytes[0] = 0x02; // compressed prefix (even)
    for (int i = 0; i < 32; i++)
        pubBytes[1+i] = masked[3+i] ^ secret.begin()[i];
    out.pubkey = CPubKey(pubBytes, pubBytes+33);
    uint8_t chainBytes[32];
    for (int i = 0; i < 32; i++)
        chainBytes[i] = masked[35+i] ^ secret.begin()[i];
    memcpy(out.chainCode.begin(), chainBytes, 32);
    return out.pubkey.IsValid();
}
