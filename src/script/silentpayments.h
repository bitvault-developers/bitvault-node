// Copyright (c) 2014-2026 The BitVault Core developers
// BIP352 Silent Payments — crypto layer (address + output creation)
#ifndef BITVAULT_SCRIPT_SILENTPAYMENTS_H
#define BITVAULT_SCRIPT_SILENTPAYMENTS_H

#include <key.h>
#include <pubkey.h>
#include <uint256.h>
#include <optional>
#include <string>
#include <vector>
#include <utility>

static constexpr const char* SILENT_PAYMENT_HRP = "bvsp";

struct SilentPaymentAddress {
    CPubKey scan_pubkey;
    CPubKey spend_pubkey;
    std::string ToString() const;
    static std::optional<SilentPaymentAddress> FromString(const std::string& addr);
    bool IsValid() const { return scan_pubkey.IsValid() && spend_pubkey.IsValid(); }
};

// Create P2TR-compatible output pubkey for a silent payment
std::optional<CPubKey> CreateSilentPaymentOutput(
    const CKey& sender_input_sum_key,
    const SilentPaymentAddress& recipient,
    const uint256& outpoint_hash,
    uint32_t counter = 0);

// Compute hash of sorted input outpoints (for ECDH binding)
uint256 ComputeOutpointHash(const std::vector<std::pair<uint256, uint32_t>>& outpoints);

// Scanning lives in wallet layer — see wallet/silentpayments.h

#endif // BITVAULT_SCRIPT_SILENTPAYMENTS_H
