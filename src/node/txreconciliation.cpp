// Copyright (c) 2014-2026 The BitVault Core developers
// BIP330 Erlay complete implementation
#include <node/txreconciliation.h>
#include <crypto/siphash.h>
#include <logging.h>
#include <random.h>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "../minisketch/include/minisketch.h"

struct PeerReconState {
    bool     registered{false};
    bool     is_inbound{false};
    uint64_t salt{0};
    std::unordered_set<uint32_t> set;
};

class TxReconciliationTracker::Impl {
public:
    uint32_t m_version;
    std::unordered_map<NodeId, uint64_t>       m_pre;
    std::unordered_map<NodeId, PeerReconState> m_peers;
    Mutex m_mutex;
    explicit Impl(uint32_t v) : m_version(v) {}
};

TxReconciliationTracker::TxReconciliationTracker(uint32_t v)
    : m_impl{std::make_unique<Impl>(v)} {}

TxReconciliationTracker::~TxReconciliationTracker() = default;

uint64_t TxReconciliationTracker::PreRegisterPeer(NodeId id) {
    LOCK(m_impl->m_mutex);
    uint64_t salt = GetRand(std::numeric_limits<uint64_t>::max());
    m_impl->m_pre[id] = salt;
    return salt;
}

ReconciliationRegisterResult TxReconciliationTracker::RegisterPeer(
    NodeId id, bool inbound, uint32_t ver, uint64_t remote_salt) {
    LOCK(m_impl->m_mutex);
    if (m_impl->m_peers.count(id)) return ReconciliationRegisterResult::ALREADY_REGISTERED;
    auto it = m_impl->m_pre.find(id);
    if (it == m_impl->m_pre.end()) return ReconciliationRegisterResult::NOT_FOUND;
    if (ver != m_impl->m_version) return ReconciliationRegisterResult::PROTOCOL_VIOLATION;
    PeerReconState s;
    s.registered = true;
    s.is_inbound = inbound;
    s.salt = it->second ^ remote_salt;
    m_impl->m_peers[id] = std::move(s);
    m_impl->m_pre.erase(it);
    return ReconciliationRegisterResult::SUCCESS;
}

void TxReconciliationTracker::ForgetPeer(NodeId id) {
    LOCK(m_impl->m_mutex);
    m_impl->m_pre.erase(id);
    m_impl->m_peers.erase(id);
}

bool TxReconciliationTracker::IsPeerRegistered(NodeId id) const {
    LOCK(m_impl->m_mutex);
    auto it = m_impl->m_peers.find(id);
    return it != m_impl->m_peers.end() && it->second.registered;
}

std::vector<uint8_t> TxReconciliationTracker::GetSketchData(NodeId id, uint16_t capacity) {
    LOCK(m_impl->m_mutex);
    auto it = m_impl->m_peers.find(id);
    if (it == m_impl->m_peers.end()) return {};
    size_t cap = std::min((size_t)capacity, (size_t)3000);
    minisketch* sk = minisketch_create(32, 0, cap);
    if (!sk) return {};
    for (uint32_t sid : it->second.set)
        minisketch_add_uint64(sk, (uint64_t)sid);
    size_t sz = minisketch_serialized_size(sk);
    std::vector<uint8_t> out(sz);
    minisketch_serialize(sk, out.data());
    minisketch_destroy(sk);
    return out;
}

bool TxReconciliationTracker::TryReconcile(NodeId id,
    const std::vector<uint8_t>& their_bytes, uint16_t capacity,
    std::vector<uint32_t>& req, std::vector<uint32_t>& ann) {
    LOCK(m_impl->m_mutex);
    req.clear(); ann.clear();
    auto it = m_impl->m_peers.find(id);
    if (it == m_impl->m_peers.end()) return false;
    size_t cap = std::min((size_t)capacity, (size_t)3000);
    if (their_bytes.size() < cap * 4) return false;
    minisketch* theirs = minisketch_create(32, 0, cap);
    if (!theirs) return false;
    minisketch_deserialize(theirs, their_bytes.data());
    minisketch* ours = minisketch_create(32, 0, cap);
    for (uint32_t sid : it->second.set)
        minisketch_add_uint64(ours, (uint64_t)sid);
    minisketch_merge(theirs, ours);
    minisketch_destroy(ours);
    std::vector<uint64_t> diff(cap);
    ssize_t n = minisketch_decode(theirs, cap, diff.data());
    minisketch_destroy(theirs);
    if (n < 0) return false;
    for (ssize_t i = 0; i < n; ++i) {
        uint32_t sid = (uint32_t)diff[i];
        if (it->second.set.count(sid)) ann.push_back(sid);
        else req.push_back(sid);
    }
    return true;
}

void TxReconciliationTracker::ClearSet(NodeId id) {
    LOCK(m_impl->m_mutex);
    auto it = m_impl->m_peers.find(id);
    if (it != m_impl->m_peers.end()) it->second.set.clear();
}
