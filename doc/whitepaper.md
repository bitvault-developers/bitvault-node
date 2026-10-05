# BitVault (BVT) - Technical Whitepaper v1.0

## Abstract
BitVault is a dual-layer blockchain combining multi-algorithm Proof of Work
with Kaspa-inspired BlockDAG technology and an EVM-compatible Layer 2.
It introduces Proof of BitVault (PoBV), a novel consensus combining
PoW + Proof of History + Delegated Proof of Stake + GHOSTDAG ordering.

## 1. Architecture
### Layer 1: Multi-Algo BlockDAG
- 11 mining algorithms rotating per block
- 1-second block time (Kaspa-style)
- GHOSTDAG parallel block acceptance
- Multi-parent block headers for DAG structure

### Layer 2: EVM Smart Contracts
- Hardhat-based EVM execution environment
- DeFi, NFT, and DAO capabilities

## 2. Proof of BitVault (PoBV)
### 2.1 PoW - 11 ASIC-resistant algorithms with DigiShield difficulty
### 2.2 PoH - 1000 SHA-256 iterations per block for time ordering
### 2.3 DPoS - 21 validators, round-robin, slashing at 100 misses
### 2.4 GHOSTDAG - K=18 cluster, blue/red scoring, selected tip

## 3. Token Economics
- Miners: 45% (32,400 BVT per block in Period I)
- Treasury: 40% (28,800 BVT)
- Staking: 15% (10,800 BVT)
- Initial reward: 72,000 BVT
- Halving: every 21,024,000 blocks (~10 years)

## 4. Network
| Param | Mainnet | Testnet | Regtest |
|-------|---------|---------|---------|
| Port | 29433 | 12026 | 29433 |
| Bech32 | bv | bvt | bcrt |
| Block Time | 1 sec | 1 sec | 1 sec |
| Algos | 11 | 11 | 11 |

## 5. Mining Algorithms
SHA256D, Scrypt, Groestl, Skein, Qubit, Equihash,
Ethash, Odo, RandomX, VersaHash, kHeavyHash

## 6. Custom RPCs
- getdaginfo - BlockDAG state
- getbluescore - GHOSTDAG blue score
- getpobvinfo - Full PoBV consensus info
- dagpruneblocks - Prune old DAG blocks

## 7. Security
- Multi-algo prevents single-hardware 51% attacks
- DPoS provides secondary block confirmation
- PoH ensures temporal ordering
- GHOSTDAG eliminates orphan blocks

## 8. Roadmap
- Phase 1: L1 Core (COMPLETE)
- Phase 2: Public Testnet
- Phase 3: L2 EVM Integration
- Phase 4: Mainnet Launch
- Phase 5: DEX, NFT, DAO ecosystem

---
BitVault Core v8.26.2 | bitvault.network
