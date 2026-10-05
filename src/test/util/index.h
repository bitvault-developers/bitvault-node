// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_TEST_UTIL_INDEX_H
#define BITVAULT_TEST_UTIL_INDEX_H

class BaseIndex;

/** Block until the index is synced to the current chain */
void IndexWaitSynced(const BaseIndex& index);

#endif // BITVAULT_TEST_UTIL_INDEX_H
