#ifndef BITVAULT_CHAINPARAMSSEEDS_H
#define BITVAULT_CHAINPARAMSSEEDS_H
/**
 * List of fixed seed nodes for the bitvault network
 *
 * Phase 1B port-fix (May 6, 2026):
 * Original 210 entries had wrong port (12024). Mainnet uses 29433.
 * Cleared until contrib/seeds/generate-seeds.py is re-run with real public IPs.
 * Daemon falls back to DNS seeds (seed.bitvault.club, dnsseed.bitvault.club).
 *
 * Each line contains a BIP155 serialized (networkID, addr, port) tuple.
 */

static const uint8_t chainparams_seed_main[] = {
    // intentionally empty — populated by contrib/seeds/generate-seeds.py
};

static const uint8_t chainparams_seed_test[] = {
    // intentionally empty
};

#endif // BITVAULT_CHAINPARAMSSEEDS_H
