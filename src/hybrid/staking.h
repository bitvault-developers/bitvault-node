#ifndef BITVAULT_HYBRID_STAKING_H
#define BITVAULT_HYBRID_STAKING_H

#include <uint256.h>
#include <vector>
#include <string>

class CStakingInterface
{
public:
    static int64_t GetTotalStake(const std::string& l1_address);
    static std::vector<std::string> GetTopValidators(int count);
    static bool VerifyCheckpointSignature(const uint256& blockHash, 
                                           const std::vector<uint8_t>& signature,
                                           const std::vector<std::string>& signers);
    static void UpdateStakeSnapshot(int nHeight);
};

#endif
