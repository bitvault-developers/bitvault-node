// Copyright (c) 2026 The BitVault Core developers
// BIP47 Reusable Payment Codes
// Allows sender to derive fresh addresses for receiver without communication
#ifndef BITVAULT_WALLET_BIP47_H
#define BITVAULT_WALLET_BIP47_H

#include <key.h>
#include <pubkey.h>
#include <script/script.h>
#include <uint256.h>
#include <vector>
#include <string>

// BIP47 Payment Code version 1
// Format: 0x47 || version(1) || features(1) || sign(1) || x(32) || chain(32) || reserved(13)
static const uint8_t BIP47_VERSION = 1;
static const uint8_t BIP47_PREFIX  = 0x47;
static const size_t  BIP47_CODE_LEN = 80;

struct PaymentCode {
    uint8_t  version;
    uint8_t  features;
    CPubKey  pubkey;
    uint256  chainCode;

    // Serialize to base58check payment code string (PM8T...)
    std::string ToString() const;
    // Parse from base58check string
    static bool FromString(const std::string& str, PaymentCode& out);
    // Derive nth notification/payment address
    bool DerivePaymentAddress(uint32_t n, CPubKey& childPub, uint256& childChain) const;
};

// BIP47 notification transaction helper
// Masks the payment code using ECDH with the notification key
std::vector<uint8_t> MaskPaymentCode(
    const PaymentCode& code,
    const CKey& senderPriv,
    const CPubKey& notifPub,
    const uint256& outpointHash
);

// Unmask received payment code
bool UnmaskPaymentCode(
    const std::vector<uint8_t>& masked,
    const CKey& recipientPriv,
    const CPubKey& senderPub,
    const uint256& outpointHash,
    PaymentCode& out
);

// Derive shared secret via ECDH for BIP47
uint256 BIP47SharedSecret(const CKey& priv, const CPubKey& pub, const uint256& outpointHash);

// Generate child key for payment n
bool BIP47DeriveChildKey(
    const CPubKey& parentPub,
    const uint256& chainCode,
    uint32_t n,
    CPubKey& childPub
);

#endif // BITVAULT_WALLET_BIP47_H
