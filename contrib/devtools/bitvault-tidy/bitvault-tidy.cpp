// Copyright (c) 2023 BitVault Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "logprintf.h"

#include <clang-tidy/ClangTidyModule.h>
#include <clang-tidy/ClangTidyModuleRegistry.h>

class BitVaultModule final : public clang::tidy::ClangTidyModule
{
public:
    void addCheckFactories(clang::tidy::ClangTidyCheckFactories& CheckFactories) override
    {
        CheckFactories.registerCheck<bitvault::LogPrintfCheck>("bitvault-unterminated-logprintf");
    }
};

static clang::tidy::ClangTidyModuleRegistry::Add<BitVaultModule>
    X("bitvault-module", "Adds bitvault checks.");

volatile int BitVaultModuleAnchorSource = 0;
