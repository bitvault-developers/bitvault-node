// BitVault Post-Quantum Crypto Initialization
// Links liboqs into bitvaultd and exposes PQC capability check
#include <oqs/oqs.h>
#include <string>
#include <logging.h>

static bool s_oqs_initialized = false;

bool BVT_PQC_Init() {
    if (s_oqs_initialized) return true;
    OQS_init();
    s_oqs_initialized = true;
    LogPrintf("liboqs %s initialized — PQC algorithms available\n", OQS_version());
    return true;
}

std::string BVT_PQC_Version() {
    return std::string(OQS_version());
}

// Force linker to keep OQS_KEM and OQS_SIG symbols
bool BVT_PQC_SelfTest() {
    // Kyber-768 (NIST PQC winner) availability check
    const OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) return false;
    OQS_KEM_free((OQS_KEM*)kem);
    return true;
}
