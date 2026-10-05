#include <script/taproot.h>
#include <crypto/schnorr.h>
#include "secp256k1/include/secp256k1.h"
#include "secp256k1/include/secp256k1_extrakeys.h"
#include <cstring>
#include <algorithm>
static secp256k1_context* TaprootCtx() {
    static secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN|SECP256K1_CONTEXT_VERIFY);
    return ctx;
}
bool XOnlyPubKey::IsValid() const {
    secp256k1_xonly_pubkey pk;
    return secp256k1_xonly_pubkey_parse(TaprootCtx(),&pk,m_keydata.data());
}
bool XOnlyPubKey::VerifySchnorr(const uint256& hash, const SchnorrSig& sig) const {
    return ::VerifySchnorr(m_keydata,hash,sig);
}
std::optional<XOnlyPubKey> XOnlyPubKey::ComputeTaprootOutputKey(const uint256* merkle_root, bool* parity_out) const {
    return ::ComputeTaprootOutputKey(*this, merkle_root?*merkle_root:uint256{}, parity_out);
}
uint256 TapLeaf::Hash() const {
    std::vector<uint8_t> data;
    data.push_back(leaf_version);
    size_t sz=script.size();
    if(sz<0xfd){data.push_back((uint8_t)sz);}
    else if(sz<=0xffff){data.push_back(0xfd);data.push_back(sz&0xff);data.push_back((sz>>8)&0xff);}
    else{data.push_back(0xfe);for(int i=0;i<4;i++)data.push_back((sz>>(8*i))&0xff);}
    data.insert(data.end(),script.begin(),script.end());
    return BIP340TaggedHash("TapLeaf", data.data(), data.size());
}
uint256 ComputeTapBranchHash(const uint256& a, const uint256& b) {
    return a<b ? BIP340TaggedHash("TapBranch",a,b) : BIP340TaggedHash("TapBranch",b,a);
}
uint256 ComputeTapTreeRoot(const std::vector<TapLeaf>& leaves) {
    if(leaves.empty()) return uint256{};
    std::vector<uint256> level;
    for(const auto& l:leaves) level.push_back(l.Hash());
    while(level.size()>1){
        std::vector<uint256> next;
        for(size_t i=0;i+1<level.size();i+=2) next.push_back(ComputeTapBranchHash(level[i],level[i+1]));
        if(level.size()%2==1) next.push_back(level.back());
        level=std::move(next);
    }
    return level[0];
}
std::optional<XOnlyPubKey> ComputeTaprootOutputKey(const XOnlyPubKey& internal_key, const uint256& merkle_root, bool* parity_out) {
    uint8_t tweak_input[64];
    memcpy(tweak_input,    internal_key.data(),32);
    memcpy(tweak_input+32, merkle_root.begin(),32);
    uint256 t = BIP340TaggedHash("TapTweak", tweak_input, 64);
    secp256k1_xonly_pubkey xpk;
    if(!secp256k1_xonly_pubkey_parse(TaprootCtx(),&xpk,internal_key.data())) return std::nullopt;
    secp256k1_pubkey pk; int parity=0;
    if(!secp256k1_xonly_pubkey_tweak_add(TaprootCtx(),&pk,&xpk,t.begin())) return std::nullopt;
    secp256k1_xonly_pubkey out_xpk;
    if(!secp256k1_xonly_pubkey_from_pubkey(TaprootCtx(),&out_xpk,&parity,&pk)) return std::nullopt;
    if(parity_out) *parity_out=(parity==1);
    uint8_t out_bytes[32];
    secp256k1_xonly_pubkey_serialize(TaprootCtx(),out_bytes,&out_xpk);
    return XOnlyPubKey(out_bytes);
}
bool VerifyTaprootCommitment(const std::vector<uint8_t>& control, const std::vector<uint8_t>& program, const uint256& tapleaf_hash) {
    if(control.size()<TAPROOT_CONTROL_BASE_SIZE) return false;
    if((control.size()-TAPROOT_CONTROL_BASE_SIZE)%TAPROOT_CONTROL_NODE_SIZE!=0) return false;
    size_t path_len=(control.size()-TAPROOT_CONTROL_BASE_SIZE)/TAPROOT_CONTROL_NODE_SIZE;
    if(path_len>TAPROOT_CONTROL_MAX_NODE_COUNT) return false;
    XOnlyPubKey internal_key(control.data()+1);
    uint256 k=tapleaf_hash;
    for(size_t i=0;i<path_len;i++){
        uint256 node; memcpy(node.begin(),control.data()+TAPROOT_CONTROL_BASE_SIZE+i*32,32);
        k=ComputeTapBranchHash(k,node);
    }
    bool expected_parity=(control[0]&1); bool actual_parity=false;
    auto output_key=ComputeTaprootOutputKey(internal_key,k,&actual_parity);
    if(!output_key) return false;
    if(program.size()!=32) return false;
    if(memcmp(output_key->data(),program.data(),32)!=0) return false;
    return actual_parity==expected_parity;
}
CScript GetP2TRScript(const XOnlyPubKey& output_key) {
    CScript s; s<<OP_1; s<<std::vector<uint8_t>(output_key.data(),output_key.data()+32); return s;
}
bool IsP2TR(const CScript& script) { return script.size()==34&&script[0]==OP_1&&script[1]==0x20; }
bool GetP2TRKey(const CScript& script, XOnlyPubKey& key_out) {
    if(!IsP2TR(script)) return false;
    key_out=XOnlyPubKey(script.data()+2); return true;
}
