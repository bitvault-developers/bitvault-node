// Copyright (c) 2009-2022 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_CLIENTVERSION_H
#define BITVAULT_CLIENTVERSION_H

#include <util/macros.h>
#include <string>
#include <vector>

#if defined(HAVE_CONFIG_H)
#include <config/bitvault-config.h>
#endif //HAVE_CONFIG_H

//! These need to be macro's as they are used in the exported header
#define CLIENT_NAME "BitVault"
#define CLIENT_VERSION_MAJOR 8
#define CLIENT_VERSION_MINOR 26
#define CLIENT_VERSION_BUILD 2
#define CLIENT_VERSION (CLIENT_VERSION_MAJOR * 10000 + CLIENT_VERSION_MINOR * 100 + CLIENT_VERSION_BUILD)

//! Set to true for release, false for prerelease or test build
#define CLIENT_VERSION_IS_RELEASE true

//! Copyright year (multi-year)
#define COPYRIGHT_YEAR 2025

//! Copyright string used in Windows .rc files
#define COPYRIGHT_STR "2009-" STRINGIFY(COPYRIGHT_YEAR) " The BitVault Core developers, 2014-" STRINGIFY(COPYRIGHT_YEAR) " The BitVault Core developers"

// Function declarations
std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);
std::string LicenseInfo();
std::string CopyrightHolders(const std::string& strPrefix);

// Windows resource file (not used on Linux)
// #include <clientversion.rc.h>

#endif // BITVAULT_CLIENTVERSION_H
