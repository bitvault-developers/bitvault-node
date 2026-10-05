#include <crypto/schnorr.h>
#include <crypto/sha256.h>
#include "secp256k1/include/secp256k1.h"
#include "secp256k1/include/secp256k1_schnorrsig.h"
#include "secp256k1/include/secp256k1_extrakeys.h"
#include <support/cleanse.h>
#include <cstring>
static secp256k1_context* GetVerifyCtx() {
    static secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    return ctx;
}
uint256 BIP340TaggedHash(const std::string& tag, const uint8_t* msg, size_t msg_len) {
    uint8_t tag_hash[32];
    CSHA256().Write((const uint8_t*)tag.data(), tag.size()).Finalize(tag_hash);
    uint256 result;
    CSHA256().Write(tag_hash,32).Write(tag_hash,32).Write(msg,msg_len).Finalize(result.begin());
    return result;
}
uint256 BIP340TaggedHash(const std::string& tag, const uint256& msg) {
    return BIP340TaggedHash(tag, msg.begin(), 32);
}
uint256 BIP340TaggedHash(const std::string& tag, const uint256& a, const uint256& b) {
    uint8_t buf[64]; memcpy(buf,a.begin(),32); memcpy(buf+32,b.begin(),32);
    return BIP340TaggedHash(tag, buf, 64);
}
bool SignSchnorr(const std::array<uint8_t,32>& seckey, const uint256& hash, SchnorrSig& sig_out, const uint8_t* aux_rand32) {
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if (!ctx) return false;
    secp256k1_keypair kp; bool ok = false;
    if (secp256k1_keypair_create(ctx, &kp, seckey.data())) {
        uint8_t aux[32]={};
        if (!aux_rand32) { CSHA256().Write(seckey.data(),32).Write(hash.begin(),32).Finalize(aux); aux_rand32=aux; }
        ok = secp256k1_schnorrsig_sign32(ctx, sig_out.data(), hash.begin(), &kp, aux_rand32);
        memory_cleanse(aux,32);
    }
    memory_cleanse(&kp,sizeof(kp)); secp256k1_context_destroy(ctx); return ok;
}
bool VerifySchnorr(const std::array<uint8_t,32>& xonly_pubkey, const uint256& hash, const SchnorrSig& sig) {
    secp256k1_xonly_pubkey pk;
    if (!secp256k1_xonly_pubkey_parse(GetVerifyCtx(), &pk, xonly_pubkey.data())) return false;
    return secp256k1_schnorrsig_verify(GetVerifyCtx(), sig.data(), hash.begin(), 32, &pk);
}
