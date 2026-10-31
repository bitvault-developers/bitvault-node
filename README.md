# BitVault (BVT) node

Node software for **BitVault**, a multi-algorithm proof-of-work Layer 1 blockchain. The coin is **BVT**.
This repository holds the source of the BitVault node, the version published below (see Releases). It follows the live network's rules, and a new node downloads the whole chain from block 0.

- Website: https://bitvault.club
- Explorer: https://explorer.bitvault.club
- Whitepaper: https://whitepaper.bitvault.club
- Supply (plain numbers): https://tracker.bitvault.club/supply/bvt/total.txt and https://tracker.bitvault.club/supply/bvt/circulating.txt
- Contact (exchanges, pools, developers): support@bitvault.club

## Network status
- Public mining opens with the official launch on **7 February 2027**. Until then the network accepts no new blocks, so the chain
  stands still at its current height and transactions do not confirm. A new node can download the whole chain and run normally now.
- Blocks up to height 43,076 (the last built-in checkpoint) were mined under earlier difficulty rules and keep the difficulty
  they were mined with. The checkpoints tie them to the one real chain, and every later block must follow today's rule.
- Every block also carries a signature from the network's validator key (held by the BitVault team). Nodes check it with the
  validator's public key, which is built into the node.

## Network parameters
| | |
|---|---|
| P2P port | 29433 |
| RPC port | 14022 (local) |
| Address format | bech32, prefix `bv` (P2WPKH, e.g. `bv1q…`) |
| HD wallet path | m/84'/0'/0' (coin type 0, as in the node's own wallet) |
| Amounts | 8 decimals |

## Build
See [BUILDING-BITVAULT.md](BUILDING-BITVAULT.md) if present, and `doc/build-unix.md`. In short, on Linux:
```
./autogen.sh
./configure --without-gui
make -C src -j"$(nproc)" bitvaultd bitvault-cli
```
Ready-made Linux binaries and their SHA-256 sums are attached to each release on this page.

## For exchanges and pools
Run `bitvaultd` with your own data folder, let it sync, and use the standard RPC calls (`getnewaddress`, `getreceivedbyaddress`,
`listtransactions`, `sendtoaddress`, `getblockcount`, …). Please contact support@bitvault.club before opening deposits:
we share seed nodes and the recommended number of confirmations.

## Licence
MIT (see `COPYING`). BitVault is built on the work of the Bitcoin Core and DigiByte Core developers; their copyright notices are kept.
