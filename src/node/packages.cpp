#include <node/packages.h>
#include <logging.h>
#include <policy/policy.h>
#include <set>

bool CheckPackageLimits(const Package& package, std::string& err) {
    if (package.empty()) { err = "empty package"; return false; }
    if (package.size() > MAX_PACKAGE_COUNT) {
        err = strprintf("too many txs: %u (max %u)", package.size(), MAX_PACKAGE_COUNT);
        return false;
    }
    size_t total = 0;
    std::set<uint256> seen;
    for (const auto& tx : package) {
        if (!tx) { err = "null tx"; return false; }
        if (!seen.insert(tx->GetHash()).second) { err = "duplicate tx"; return false; }
        total += GetVirtualTransactionSize(*tx);
    }
    if (total * WITNESS_SCALE_FACTOR > MAX_PACKAGE_WEIGHT) {
        err = strprintf("package too large: %u vB", total); return false;
    }
    return true;
}
