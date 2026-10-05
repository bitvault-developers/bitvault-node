// Copyright (c) 2014-2026 The BitVault Core developers
// BIP324 v2 encrypted P2P transport
#ifndef BITVAULT_TRANSPORT_H
#define BITVAULT_TRANSPORT_H

#include <crypto/chacha20poly1305.h>
#include <crypto/hkdf_sha256_32.h>
#include <span.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr size_t V2_ELLSWIFT_LEN = 64;
static constexpr size_t V2_MAC_LEN      = 16;  // Poly1305::TAGLEN
static constexpr uint32_t V2_REKEY_INTERVAL = 224;

enum class V2State { HANDSHAKE_SEND, HANDSHAKE_RECV, READY };

class V2Transport {
public:
    explicit V2Transport(bool initiator);

    // Bytes to send at connection start (our ell64 pubkey, no garbage for simplicity)
    std::vector<uint8_t> GetHandshakeBytes() const;

    // Feed received handshake bytes; returns true when READY
    bool ProcessHandshake(const uint8_t* data, size_t len);

    bool IsReady() const { return m_state == V2State::READY; }

    // Encrypt outbound: returns [4-byte length][ciphertext+MAC]
    std::vector<uint8_t> EncryptMessage(const std::string& cmd,
                                         const std::vector<uint8_t>& payload);

    // Feed raw bytes into recv buffer
    void FeedRecv(const uint8_t* data, size_t len) {
        m_recv_buf.insert(m_recv_buf.end(), data, data + len);
    }

    // Try to decrypt one message; returns {cmd, payload} or nullopt
    std::optional<std::pair<std::string, std::vector<uint8_t>>> DecryptMessage();

private:
    bool m_initiator;
    V2State m_state{V2State::HANDSHAKE_SEND};

    std::array<uint8_t, 32>              m_seckey{};
    std::array<uint8_t, V2_ELLSWIFT_LEN> m_our_ell64{};

    std::unique_ptr<FSChaCha20Poly1305> m_send_aead;
    std::unique_ptr<FSChaCha20Poly1305> m_recv_aead;

    std::vector<uint8_t> m_recv_buf;
    std::vector<uint8_t> m_their_ell64_buf;

    bool DeriveSessionKeys();

    static const std::unordered_map<std::string, uint8_t> s_cmd_to_id;
    static const std::unordered_map<uint8_t, std::string> s_id_to_cmd;
};

#endif // BITVAULT_TRANSPORT_H
