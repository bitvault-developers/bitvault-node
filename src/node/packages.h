// Copyright (c) 2014-2026 The BitVault Core developers
// BIP331 Package Relay — uses existing policy/packages.h infrastructure
#ifndef BITVAULT_NODE_PACKAGES_H
#define BITVAULT_NODE_PACKAGES_H

#include <policy/packages.h>
#include <primitives/transaction.h>
#include <string>

// BIP331 wire constants (avoid autoconf macro names)
static constexpr uint8_t  PKG_RELAY_VERSION   = 1;
static constexpr uint8_t  PKG_TYPE_CWUP       = 0x01;

// Validate package size/count limits
bool CheckPackageLimits(const Package& package, std::string& err);

#endif // BITVAULT_NODE_PACKAGES_H
