// Copyright (c) 2014-2026 The BitVault Core developers
// Distributed under the MIT software license.
// RPC endpoints: MuSig2 + Bulletproofs + Taproot Assets + Silent Payments
#include <rpc/server.h>
#include <rpc/util.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <crypto/musig2.h>
#include <crypto/bulletproofs.h>
#include <taproot_assets/taproot_assets.h>
#include <silentpayments/bip352_scanner.h>
#include <array>
#include <vector>
#include <string>
template <typename Arr>
static std::string ToHex(const Arr& a) {
    return HexStr(Span<const unsigned char>(reinterpret_cast<const unsigned char*>(a.data()), a.size()));
}
static RPCHelpMan musig2_taggedhash() {
    return RPCHelpMan{"musig2_taggedhash","\nBIP340 tagged hash\n",
        {{"tag",RPCArg::Type::STR,RPCArg::Optional::NO,"Tag string"},
         {"data_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"Data (hex)"}},
        RPCResult{RPCResult::Type::STR_HEX,"hash","32-byte hash"},
        RPCExamples{HelpExampleCli("musig2_taggedhash","\"BIP0340/challenge\" \"deadbeef\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            auto v=ParseHex(request.params[1].get_str());
            std::vector<uint8_t> d(v.begin(),v.end());
            return ToHex(BitVault::MuSig2::taggedHash(request.params[0].get_str(),d));
        }};
}
static RPCHelpMan musig2_challengehash() {
    return RPCHelpMan{"musig2_challengehash","\nBIP327 Schnorr challenge e=H(R||P||msg)\n",
        {{"R_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"33-byte R (hex)"},
         {"P_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"33-byte P (hex)"},
         {"msg_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"32-byte msg (hex)"}},
        RPCResult{RPCResult::Type::STR_HEX,"challenge","32-byte challenge"},
        RPCExamples{HelpExampleCli("musig2_challengehash","\"<R>\" \"<P>\" \"<msg>\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            using namespace BitVault::MuSig2;
            auto rv=ParseHex(request.params[0].get_str()),pv=ParseHex(request.params[1].get_str()),mv=ParseHex(request.params[2].get_str());
            if(rv.size()!=33||pv.size()!=33) throw JSONRPCError(RPC_INVALID_PARAMETER,"R and P must be 33 bytes");
            if(mv.size()!=32) throw JSONRPCError(RPC_INVALID_PARAMETER,"msg must be 32 bytes");
            PubKey33 R{},P{}; Bytes32 M{};
            std::copy(rv.begin(),rv.end(),R.begin()); std::copy(pv.begin(),pv.end(),P.begin()); std::copy(mv.begin(),mv.end(),M.begin());
            return ToHex(BitVault::MuSig2::challengeHash(R,P,M));
        }};
}
static RPCHelpMan bulletproofs_commit() {
    return RPCHelpMan{"bulletproofs_commit","\nPedersen commitment C=value*G+blinding*H\n",
        {{"value",RPCArg::Type::STR,RPCArg::Optional::NO,"Non-negative integer"},
         {"blinding_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"32-byte blinding (hex)"},
         {"range_bits",RPCArg::Type::NUM,RPCArg::Optional::OMITTED,"Bit range (default 64)"}},
        RPCResult{RPCResult::Type::OBJ,"","",{
            {RPCResult::Type::STR_HEX,"commitment","33-byte point (hex)"},
            {RPCResult::Type::NUM,"value","Committed value"}}},
        RPCExamples{HelpExampleCli("bulletproofs_commit","1000 \"<32-byte-hex>\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            using namespace BitVault::Bulletproofs;
            uint64_t val=request.params[0].isNum()?(uint64_t)request.params[0].getInt<int64_t>():(uint64_t)std::stoull(request.params[0].get_str());
            auto bv=ParseHex(request.params[1].get_str());
            if(bv.size()!=32) throw JSONRPCError(RPC_INVALID_PARAMETER,"blinding must be 32 bytes");
            size_t bits=request.params[2].isNull()?64:(size_t)request.params[2].getInt<int>();
            Bytes32 b{}; std::copy(bv.begin(),bv.end(),b.begin());
            auto gens=setupGenerators(bits); auto c=commit(val,b,gens);
            UniValue r(UniValue::VOBJ); r.pushKV("commitment",ToHex(c.point)); r.pushKV("value",(uint64_t)val);
            return r;
        }};
}
static RPCHelpMan silentpayment_encode() {
    return RPCHelpMan{"silentpayment_encode","\nEncode BIP352 silent payment address (bvsp1...)\n",
        {{"scan_pubkey_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"33-byte scan pubkey (hex)"},
         {"spend_pubkey_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"33-byte spend pubkey (hex)"}},
        RPCResult{RPCResult::Type::STR,"address","bvsp1... silent payment address"},
        RPCExamples{HelpExampleCli("silentpayment_encode","\"<scan-hex>\" \"<spend-hex>\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            using namespace BitVault::SilentPayments;
            auto sv=ParseHex(request.params[0].get_str()),spv=ParseHex(request.params[1].get_str());
            if(sv.size()!=33||spv.size()!=33) throw JSONRPCError(RPC_INVALID_PARAMETER,"Both pubkeys must be 33 bytes");
            SilentPaymentAddress addr{};
            std::copy(sv.begin(),sv.end(),addr.scanPubKey.begin());
            std::copy(spv.begin(),spv.end(),addr.spendPubKey.begin());
            return BIP352Scanner::encodeAddress(addr);
        }};
}
static RPCHelpMan silentpayment_decode() {
    return RPCHelpMan{"silentpayment_decode","\nDecode a bvsp1... silent payment address\n",
        {{"address",RPCArg::Type::STR,RPCArg::Optional::NO,"bvsp1... address"}},
        RPCResult{RPCResult::Type::OBJ,"","",{
            {RPCResult::Type::STR_HEX,"scan_pubkey","33-byte scan pubkey (hex)"},
            {RPCResult::Type::STR_HEX,"spend_pubkey","33-byte spend pubkey (hex)"}}},
        RPCExamples{HelpExampleCli("silentpayment_decode","\"bvsp1...\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            using namespace BitVault::SilentPayments;
            auto addr=BIP352Scanner::decodeAddress(request.params[0].get_str());
            UniValue r(UniValue::VOBJ); r.pushKV("scan_pubkey",ToHex(addr.scanPubKey)); r.pushKV("spend_pubkey",ToHex(addr.spendPubKey));
            return r;
        }};
}
static RPCHelpMan taprootasset_computescriptkey() {
    return RPCHelpMan{"taprootasset_computescriptkey","\nCompute P2TR script key for Taproot Asset owner\n",
        {{"owner_pubkey_hex",RPCArg::Type::STR_HEX,RPCArg::Optional::NO,"33-byte owner pubkey (hex)"}},
        RPCResult{RPCResult::Type::STR_HEX,"script_key","32-byte tweaked key (hex)"},
        RPCExamples{HelpExampleCli("taprootasset_computescriptkey","\"<owner-hex>\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            using namespace BitVault::TaprootAssets;
            auto ov=ParseHex(request.params[0].get_str());
            if(ov.size()!=33) throw JSONRPCError(RPC_INVALID_PARAMETER,"owner_pubkey_hex must be 33 bytes");
            PubKey33 owner{}; std::copy(ov.begin(),ov.end(),owner.begin());
            AssetScript script; script.type=AssetScript::KEY_PATH; script.threshold=1; script.locktime=0; script.keys.push_back(owner);
            return ToHex(computeScriptKey(owner,script));
        }};
}
void RegisterBVTCryptoRPCCommands(CRPCTable& tableRPC) {
    static const CRPCCommand commands[]{
        {"bvtcrypto",&musig2_taggedhash},
        {"bvtcrypto",&musig2_challengehash},
        {"bvtcrypto",&bulletproofs_commit},
        {"bvtcrypto",&silentpayment_encode},
        {"bvtcrypto",&silentpayment_decode},
        {"bvtcrypto",&taprootasset_computescriptkey},
    };
    for(const auto& c:commands) tableRPC.appendCommand(c.name,&c);
}
