#ifndef BITVAULT_GOVERNANCE_H
#define BITVAULT_GOVERNANCE_H

#include <string>
#include <cstdint>

// ====== Feature: L1 On-Chain Governance ======
// Miners signal votes via OP_RETURN: "BV_VOTE:ID:yes/no"

static const int GOVERNANCE_VOTE_WINDOW = 10080;
static const int GOVERNANCE_THRESHOLD_PERCENT = 75;

inline bool ParseGovernanceVote(const std::string& data, uint32_t& proposalId, bool& voteYes) {
    if (data.substr(0, 8) != "BV_VOTE:") return false;
    size_t colonPos = data.find(':', 8);
    if (colonPos == std::string::npos) return false;
    try { proposalId = std::stoul(data.substr(8, colonPos - 8)); }
    catch (...) { return false; }
    std::string vote = data.substr(colonPos + 1);
    if (vote == "yes") { voteYes = true; return true; }
    if (vote == "no") { voteYes = false; return true; }
    return false;
}

inline bool HasProposalPassed(int yes, int no) {
    int total = yes + no;
    if (total == 0) return false;
    return (yes * 100 / total) >= GOVERNANCE_THRESHOLD_PERCENT;
}

#endif
