// Copyright (c) 2025 The BitVault Core developers
// GHOSTDAG - Greedy Heaviest Observed Sub-DAG ordering
#ifndef BITVAULT_DAG_GHOSTDAG_H
#define BITVAULT_DAG_GHOSTDAG_H

#include <uint256.h>
#include <sync.h>
#include <map>
#include <vector>
#include <set>

struct DAGBlock {
    uint256 hash;
    int height;
    uint256 hashPrevBlock;
    std::vector<uint256> parentHashes;
    int64_t blueScore;
    bool isBlue;
    int64_t nTime;
    DAGBlock() : height(0), blueScore(0), isBlue(true), nTime(0) {}
};

class GHOSTDAGManager {
private:
    mutable RecursiveMutex cs_dag;
    std::map<uint256, DAGBlock> dagBlocks;
    uint256 selectedTip;
    int kCluster;
public:
    GHOSTDAGManager(int k = 18) : kCluster(k) {}
    bool AddBlock(const uint256& hash, int height, const uint256& prevHash,
                  const std::vector<uint256>& parents, int64_t nTime) {
        LOCK(cs_dag);
        if (dagBlocks.count(hash)) return false;
        DAGBlock block;
        block.hash = hash; block.height = height;
        block.hashPrevBlock = prevHash; block.parentHashes = parents;
        block.nTime = nTime;
        int64_t maxPS = 0; int bluePC = 0;
        for (const auto& ph : parents) {
            auto it = dagBlocks.find(ph);
            if (it != dagBlocks.end()) {
                if (it->second.blueScore > maxPS) maxPS = it->second.blueScore;
                if (it->second.isBlue) bluePC++;
            }
        }
        auto prevIt = dagBlocks.find(prevHash);
        if (prevIt != dagBlocks.end()) {
            if (prevIt->second.blueScore > maxPS) maxPS = prevIt->second.blueScore;
            if (prevIt->second.isBlue) bluePC++;
        }
        block.blueScore = maxPS + 1;
        block.isBlue = (bluePC > 0 || dagBlocks.empty());
        dagBlocks[hash] = block;
        bool updateTip = selectedTip.IsNull();
        if (not updateTip) updateTip = block.blueScore > dagBlocks[selectedTip].blueScore;
        if (not updateTip) updateTip = (block.blueScore == dagBlocks[selectedTip].blueScore and hash < selectedTip);
        if (updateTip) selectedTip = hash;
        return true;
    }
    uint256 GetSelectedTip() const { LOCK(cs_dag); return selectedTip; }
    int64_t GetBlueScore(const uint256& hash) const {
        LOCK(cs_dag);
        auto it = dagBlocks.find(hash);
        return (it != dagBlocks.end()) ? it->second.blueScore : -1;
    }
    bool IsBlue(const uint256& hash) const {
        LOCK(cs_dag);
        auto it = dagBlocks.find(hash);
        return (it != dagBlocks.end()) ? it->second.isBlue : false;
    }
    std::vector<uint256> GetTips() const {
        LOCK(cs_dag);
        std::set<uint256> referenced;
        for (const auto& [h, b] : dagBlocks) {
            referenced.insert(b.hashPrevBlock);
            for (const auto& p : b.parentHashes) referenced.insert(p);
        }
        std::vector<uint256> tips;
        for (const auto& [h, b] : dagBlocks) {
            if (referenced.find(h) == referenced.end()) tips.push_back(h);
        }
        if (tips.empty() and (selectedTip.IsNull() == false)) tips.push_back(selectedTip);
        return tips;
    }
    size_t GetBlockCount() const { LOCK(cs_dag); return dagBlocks.size(); }
    bool GetBlockInfo(const uint256& hash, DAGBlock& out) const {
        LOCK(cs_dag);
        auto it = dagBlocks.find(hash);
        if (it == dagBlocks.end()) return false;
        out = it->second; return true;
    }
    std::vector<uint256> GetBlocksAtHeight(int height) const {
        LOCK(cs_dag);
        std::vector<uint256> result;
        for (const auto& [h, b] : dagBlocks) {
            if (b.height == height) result.push_back(h);
        }
        return result;
    }
    int PruneBelow(int height) {
        LOCK(cs_dag);
        int pruned = 0;
        for (auto it = dagBlocks.begin(); it != dagBlocks.end(); ) {
            if (it->second.height < height) { it = dagBlocks.erase(it); pruned++; }
            else { ++it; }
        }
        return pruned;
    }
};

#endif // BITVAULT_DAG_GHOSTDAG_H
