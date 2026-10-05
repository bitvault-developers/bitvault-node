#ifndef BITVAULT_CRYPTO_SCHNORR_H
#define BITVAULT_CRYPTO_SCHNORR_H
#include <array>
#include <cstdint>
#include <string>
#include <uint256.h>
using SchnorrSig = std::array<uint8_t, 64>;
uint256 BIP340TaggedHash(const std::string& tag, const uint8_t* msg, size_t msg_len);
uint256 BIP340TaggedHash(const std::string& tag, const uint256& msg);
uint256 BIP340TaggedHash(const std::string& tag, const uint256& a, const uint256& b);
bool SignSchnorr(const std::array<uint8_t,32>& seckey, const uint256& hash, SchnorrSig& sig_out, const uint8_t* aux_rand32 = nullptr);
bool VerifySchnorr(const std::array<uint8_t,32>& xonly_pubkey, const uint256& hash, const SchnorrSig& sig);
#endif
