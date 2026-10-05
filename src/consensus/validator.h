// Copyright (c) 2025 The BitVault Core developers
// DPoS Validator Set - registration, rotation, slashing
#ifndef BITVAULT_CONSENSUS_VALIDATOR_H
#define BITVAULT_CONSENSUS_VALIDATOR_H

#include <key.h>
#include <pubkey.h>
#include <uint256.h>
#include <sync.h>
#include <map>
#include <vector>
#include <set>

struct Validator {
    CPubKey pubkey;
    CAmount stake;          // staked BVT amount
    int64_t registeredHeight;
    int64_t lastBlockHeight; // last block signed
    int missedBlocks;        // consecutive missed blocks
    bool slashed;
    Validator() : stake(0), registeredHeight(0), lastBlockHeight(0), missedBlocks(0), slashed(false) {}
};

class ValidatorSet {
private:
    mutable RecursiveMutex cs_validators;
    std::map<CPubKey, Validator> validators;
    int maxValidators;
    int slashThreshold; // missed blocks before slashing
    CAmount minStake;

public:
    ValidatorSet(int maxVal = 21, int slashThresh = 100, CAmount minS = 100000 * 100000000LL)
        : maxValidators(maxVal), slashThreshold(slashThresh), minStake(minS) {
        // Register default regtest validator
        const CPubKey k(ParseHex("035d1dbc63ad123e51f972886e6c3f71c350331d5e15f37cf69f01e2e4e82b82f2")); // BVT_OLD_KEY_REMOVED: public key only
        if (k.IsFullyValid()) {
            Validator v; v.pubkey = k; v.stake = minS; v.registeredHeight = 0;
            validators[v.pubkey] = v;
        }
    }

    bool RegisterValidator(const CPubKey& pubkey, CAmount stake, int height) {
        LOCK(cs_validators);
        if (stake < minStake) return false;
        if ((int)validators.size() >= maxValidators) return false;
        if (validators.count(pubkey)) return false;
        Validator v; v.pubkey = pubkey; v.stake = stake; v.registeredHeight = height;
        validators[pubkey] = v;
        return true;
    }

    bool IsValidator(const CPubKey& pubkey) const {
        LOCK(cs_validators);
        auto it = validators.find(pubkey);
        return it != validators.end() and not it->second.slashed;
    }

    CPubKey GetCurrentValidator(int height) const {
        LOCK(cs_validators);
        std::vector<CPubKey> active;
        for (const auto& [pk, v] : validators) {
            if (not v.slashed) active.push_back(pk);
        }
        if (active.empty()) return CPubKey();
        return active[height % active.size()]; // round-robin rotation
    }

    void RecordBlock(const CPubKey& pubkey, int height) {
        LOCK(cs_validators);
        auto it = validators.find(pubkey);
        if (it != validators.end()) {
            it->second.lastBlockHeight = height;
            it->second.missedBlocks = 0;
        }
    }

    void RecordMiss(const CPubKey& pubkey) {
        LOCK(cs_validators);
        auto it = validators.find(pubkey);
        if (it != validators.end()) {
            it->second.missedBlocks++;
            if (it->second.missedBlocks >= slashThreshold) {
                it->second.slashed = true;
            }
        }
    }

    bool UnregisterValidator(const CPubKey& pubkey) {
        LOCK(cs_validators);
        return validators.erase(pubkey) > 0;
    }

    size_t GetValidatorCount() const {
        LOCK(cs_validators);
        size_t count = 0;
        for (const auto& [pk, v] : validators) { if (not v.slashed) count++; }
        return count;
    }

    std::vector<Validator> GetAllValidators() const {
        LOCK(cs_validators);
        std::vector<Validator> result;
        for (const auto& [pk, v] : validators) result.push_back(v);
        return result;
    }
};

#endif // BITVAULT_CONSENSUS_VALIDATOR_H
