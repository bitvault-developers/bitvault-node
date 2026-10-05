// Copyright (c) 2014-2026 The BitVault Core developers
// BIP324 v2 encrypted P2P transport
#include <transport.h>
#include <logging.h>
#include <random.h>
#include <support/cleanse.h>
#include <cstring>
#include "secp256k1/include/secp256k1.h"
#include "secp256k1/include/secp256k1_ellswift.h"

const std::unordered_map<std::string, uint8_t> V2Transport::s_cmd_to_id = {
    {"version",0},{"verack",1},{"addr",2},{"addrv2",3},{"inv",4},
    {"getdata",5},{"notfound",6},{"getblocks",7},{"getheaders",8},
    {"tx",9},{"block",10},{"headers",11},{"sendheaders",12},
    {"getaddr",13},{"ping",14},{"pong",15},{"mempool",16},
    {"filterload",17},{"filteradd",18},{"filterclear",19},
    {"feefilter",20},{"sendcmpct",21},{"cmpctblock",22},
    {"getblocktxn",23},{"blocktxn",24},
};
const std::unordered_map<uint8_t, std::string> V2Transport::s_id_to_cmd = [](){
    std::unordered_map<uint8_t, std::string> m;
    for (auto& [k,v] : V2Transport::s_cmd_to_id) m[v]=k;
    return m;
}();

static secp256k1_context* GetECCtx() {
    static secp256k1_context* ctx =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return ctx;
}

V2Transport::V2Transport(bool initiator) : m_initiator(initiator) {
    while (true) {
        // GetRandBytes takes Span<unsigned char>
        GetRandBytes(m_seckey);
        std::array<uint8_t, 32> rnd{};
        GetRandBytes(rnd);
        if (secp256k1_ellswift_create(GetECCtx(),
                m_our_ell64.data(), m_seckey.data(), rnd.data())) break;
    }
    m_state = V2State::HANDSHAKE_SEND;
}

std::vector<uint8_t> V2Transport::GetHandshakeBytes() const {
    return std::vector<uint8_t>(m_our_ell64.begin(), m_our_ell64.end());
}

bool V2Transport::ProcessHandshake(const uint8_t* data, size_t len) {
    m_their_ell64_buf.insert(m_their_ell64_buf.end(), data, data + len);
    if (m_their_ell64_buf.size() < V2_ELLSWIFT_LEN) return false;
    FeedRecv(m_their_ell64_buf.data() + V2_ELLSWIFT_LEN,
             m_their_ell64_buf.size() - V2_ELLSWIFT_LEN);
    m_their_ell64_buf.resize(V2_ELLSWIFT_LEN);
    if (!DeriveSessionKeys()) return false;
    m_state = V2State::READY;
    LogPrint(BCLog::NET, "BIP324: v2 session established (initiator=%d)\n", (int)m_initiator);
    return true;
}

bool V2Transport::DeriveSessionKeys() {
    uint8_t shared[32];
    const uint8_t* ell_A = m_initiator ? m_our_ell64.data()      : m_their_ell64_buf.data();
    const uint8_t* ell_B = m_initiator ? m_their_ell64_buf.data(): m_our_ell64.data();
    int party = m_initiator ? 0 : 1;

    if (!secp256k1_ellswift_xdh(GetECCtx(), shared,
            ell_A, ell_B, m_seckey.data(), party,
            secp256k1_ellswift_xdh_hash_function_bip324, nullptr)) {
        return false;
    }

    CHKDF_HMAC_SHA256_L32 hkdf(shared, 32, "");
    uint8_t k_init[32], k_resp[32];
    hkdf.Expand32("initiator_K", k_init);
    hkdf.Expand32("responder_K", k_resp);

    const uint8_t* sk = m_initiator ? k_init : k_resp;
    const uint8_t* rk = m_initiator ? k_resp : k_init;

    m_send_aead = std::make_unique<FSChaCha20Poly1305>(
        Span<const std::byte>{reinterpret_cast<const std::byte*>(sk), 32},
        V2_REKEY_INTERVAL);
    m_recv_aead = std::make_unique<FSChaCha20Poly1305>(
        Span<const std::byte>{reinterpret_cast<const std::byte*>(rk), 32},
        V2_REKEY_INTERVAL);

    memory_cleanse(shared, 32);
    memory_cleanse(k_init, 32);
    memory_cleanse(k_resp, 32);
    return true;
}

std::vector<uint8_t> V2Transport::EncryptMessage(const std::string& cmd,
                                                   const std::vector<uint8_t>& payload) {
    if (!m_send_aead) return {};
    std::vector<uint8_t> plain;
    auto it = s_cmd_to_id.find(cmd);
    if (it != s_cmd_to_id.end()) {
        plain.push_back(static_cast<uint8_t>(it->second + 1));
    } else {
        plain.push_back(0);
        plain.resize(1 + 12, 0);
        memcpy(&plain[1], cmd.data(), std::min(cmd.size(), size_t{12}));
    }
    plain.insert(plain.end(), payload.begin(), payload.end());
    size_t pt_len = plain.size();
    std::vector<uint8_t> out(4 + pt_len + V2_MAC_LEN);
    out[0]=pt_len&0xFF; out[1]=(pt_len>>8)&0xFF;
    out[2]=(pt_len>>16)&0xFF; out[3]=(pt_len>>24)&0xFF;
    m_send_aead->Encrypt(
        Span<const std::byte>{reinterpret_cast<const std::byte*>(plain.data()), pt_len},
        Span<const std::byte>{},
        Span<std::byte>{reinterpret_cast<std::byte*>(out.data()+4), pt_len+V2_MAC_LEN});
    return out;
}

std::optional<std::pair<std::string,std::vector<uint8_t>>> V2Transport::DecryptMessage() {
    if (!m_recv_aead || m_recv_buf.size() < 4) return std::nullopt;
    uint32_t pt_len = m_recv_buf[0]|(m_recv_buf[1]<<8)|
                      (m_recv_buf[2]<<16)|(m_recv_buf[3]<<24);
    if (m_recv_buf.size() < 4+pt_len+V2_MAC_LEN) return std::nullopt;
    std::vector<uint8_t> plain(pt_len);
    bool ok = m_recv_aead->Decrypt(
        Span<const std::byte>{reinterpret_cast<const std::byte*>(
            m_recv_buf.data()+4), pt_len+V2_MAC_LEN},
        Span<const std::byte>{},
        Span<std::byte>{reinterpret_cast<std::byte*>(plain.data()), pt_len});
    m_recv_buf.erase(m_recv_buf.begin(), m_recv_buf.begin()+4+pt_len+V2_MAC_LEN);
    if (!ok || plain.empty()) return std::nullopt;
    std::string cmd; size_t ps=1;
    if (plain[0]==0) {
        if (plain.size()<13) return std::nullopt;
        cmd=std::string(reinterpret_cast<const char*>(&plain[1]),
                        strnlen(reinterpret_cast<const char*>(&plain[1]),12));
        ps=13;
    } else {
        auto jt=s_id_to_cmd.find((uint8_t)(plain[0]-1));
        if (jt==s_id_to_cmd.end()) return std::nullopt;
        cmd=jt->second;
    }
    return std::make_pair(cmd,std::vector<uint8_t>(plain.begin()+ps,plain.end()));
}
