// Copyright (c) 2022 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_KERNEL_VALIDATION_CACHE_SIZES_H
#define BITVAULT_KERNEL_VALIDATION_CACHE_SIZES_H

#include <script/sigcache.h>

#include <cstddef>
#include <limits>

namespace kernel {
struct ValidationCacheSizes {
    size_t signature_cache_bytes{DEFAULT_MAX_SIG_CACHE_BYTES / 2};
    size_t script_execution_cache_bytes{DEFAULT_MAX_SIG_CACHE_BYTES / 2};
};
}

#endif // BITVAULT_KERNEL_VALIDATION_CACHE_SIZES_H
