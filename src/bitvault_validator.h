// BVT_POW80 chain upgrade: proof of work = hash of the 80-byte base header; new validator key.
#ifndef BITVAULT_VALIDATOR_H
#define BITVAULT_VALIDATOR_H
#include <cstdint>
static constexpr int64_t BVT_POW80_ACTIVATION_TIME = 1790787600;  // block time (UTC) from which the upgrade rules apply
static constexpr const char* BVT_VALIDATOR_PUBKEY = "02bb3db858ba65ef251994e997663375c742c48bc4dcec2e79411fd60a04fc08d8";  // new validator (private key only in <datadir>/validator.key)
static constexpr const char* BVT_OLD_VALIDATOR_PUBKEY = "035d1dbc63ad123e51f972886e6c3f71c350331d5e15f37cf69f01e2e4e82b82f2";  // signed every block before the upgrade
static constexpr const char* BVT_REGTEST_VALIDATOR_PUBKEY = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798";  // private key 1, for tests only
#endif
