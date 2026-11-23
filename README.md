# BitVault (BVT) node

Node software for **BitVault**, a multi-algorithm proof-of-work Layer 1 blockchain. The coin is **BVT**.
This repository holds the source of the BitVault node and the ready-made Linux programs (see Releases).
A new node downloads the whole chain from block 0 over the normal network protocol.

- Website: https://bitvault.club
- Exchange integration guide (node, deposits, withdrawals, fees, supply): https://bitvault.club/integrate/
- Explorer: https://explorer.bitvault.club
- Whitepaper: https://whitepaper.bitvault.club
- Supply (plain numbers): https://tracker.bitvault.club/supply/bvt/total.txt and https://tracker.bitvault.club/supply/bvt/circulating.txt
- Security issues: see [SECURITY.md](SECURITY.md)
- Contact (exchanges, pools, developers): support@bitvault.club

## Network status
- Mainnet has run since 28 August 2026. Block production paused at block 43,076 and **resumed on 21 November 2026**
  (first new block 43,077) with node **v8.26.13**. Transactions confirm normally.
- Until the official launch on **7 February 2027**, blocks are produced by the BitVault team's node and public pool mining
  is closed. From the launch, mining opens to everyone through BVT Pool (bitvaultpool.com) and solo mining.
- Every block carries a signature from the network's validator key (held by the BitVault team). Nodes check it with the
  validator's public key, which is built into the node.
- Blocks up to height 43,076 (the last built-in checkpoint) were mined under earlier difficulty rules and keep the
  difficulty they were mined with. The checkpoints tie them to the one real chain, and every later block follows today's rule.
- **Use v8.26.13 or newer.** v8.26.11 and v8.26.12 stop at block 43,076.

## Network parameters
| | |
|---|---|
| Ticker | BVT (8 decimals) |
| Consensus | Proof of work, multi-algorithm, plus validator signature |
| Target block time | 60 seconds |
| Block reward | 3 BVT per block plus a diversity bonus of up to 10% (from block 43,077) |
| New-coin maturity | 100 blocks |
| P2P port | 29433 |
| RPC port | 14022 (keep it on localhost) |
| Seeds | seed.bitvault.club, dnsseed.bitvault.club |
| Address format | bech32, prefix `bv` (native SegWit `bv1q…`, Taproot `bv1p…`) |
| HD wallet path | m/84'/0'/0' (coin type 0, as in the node's own wallet) |
| Recommended deposit confirmations | 10 |

**Fees.** The node's built-in wallet pays at least 0.1 BVT per kvB (10,000 sat/vB), so a normal payment of about 141 vB
costs about 0.0141 BVT. With default settings a node relays transactions from 100 sat/vB.
Full replace-by-fee is on, so never credit unconfirmed payments.

**Version label.** The v8.26.13 program still reports the old label `8.26.2` (`/BitVault:8.26.2/`, protocol 70019).
Identify the build by its SHA-256 in the release's SHA256SUMS. The label will be corrected in the next release.

## Download and check
Download `bitvaultd`, `bitvault-cli` and `SHA256SUMS` from the latest release on this page, then:
```
sha256sum -c SHA256SUMS --ignore-missing
```
Every listed file must say `OK`.

## Build
See [BUILDING-BITVAULT.md](BUILDING-BITVAULT.md) and `doc/build-unix.md`. In short, on Linux:
```
./autogen.sh
./configure --without-gui
make -C src -j"$(nproc)" bitvaultd bitvault-cli
```

## For exchanges and pools
Follow the integration guide at https://bitvault.club/integrate/. In short: run `bitvaultd` with your own data folder,
let it sync, and use the standard RPC calls (`getnewaddress "" bech32`, `listsinceblock`, `sendtoaddress`,
`getblockcount`, …). Write to support@bitvault.club for test transactions or a second node to peer with.

## Licence
MIT (see `COPYING`). BitVault is built on the work of the Bitcoin Core and DigiByte Core developers; their copyright notices are kept.
