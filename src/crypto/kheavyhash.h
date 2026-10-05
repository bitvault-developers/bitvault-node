#ifndef KHEAVYHASH_H
#define KHEAVYHASH_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdlib>

// KHeavyHash — 11th Mining Algorithm for BitVault
// Based on Kaspa's optical-mining-friendly KHeavyHash
// Steps: SHA3-256 → 64x64 Matrix Multiply → XOR → SHA3-256

namespace kheavyhash {

// Simple SHA3-256 (Keccak) implementation for standalone use
static const uint64_t KECCAK_RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

inline uint64_t rotl64(uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }

inline void keccak_f1600(uint64_t state[25]) {
    for (int round = 0; round < 24; round++) {
        // Theta
        uint64_t C[5], D[5];
        for (int i = 0; i < 5; i++)
            C[i] = state[i] ^ state[i+5] ^ state[i+10] ^ state[i+15] ^ state[i+20];
        for (int i = 0; i < 5; i++) {
            D[i] = C[(i+4)%5] ^ rotl64(C[(i+1)%5], 1);
            for (int j = 0; j < 25; j += 5) state[j+i] ^= D[i];
        }
        // Rho and Pi
        uint64_t temp = state[1];
        static const int piln[24] = {10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};
        static const int rotc[24] = {1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
        for (int i = 0; i < 24; i++) {
            uint64_t t = state[piln[i]];
            state[piln[i]] = rotl64(temp, rotc[i]);
            temp = t;
        }
        // Chi
        for (int j = 0; j < 25; j += 5) {
            uint64_t t[5];
            for (int i = 0; i < 5; i++) t[i] = state[j+i];
            for (int i = 0; i < 5; i++)
                state[j+i] = t[i] ^ ((~t[(i+1)%5]) & t[(i+2)%5]);
        }
        // Iota
        state[0] ^= KECCAK_RC[round];
    }
}

inline void sha3_256(const uint8_t* input, size_t len, uint8_t* output) {
    uint64_t state[25] = {0};
    size_t rate = 136; // SHA3-256 rate = 1088 bits = 136 bytes
    
    // Absorb
    size_t offset = 0;
    while (offset + rate <= len) {
        for (size_t i = 0; i < rate/8; i++)
            state[i] ^= ((const uint64_t*)(input + offset))[i];
        keccak_f1600(state);
        offset += rate;
    }
    
    // Padding
    uint8_t pad[136] = {0};
    size_t remaining = len - offset;
    memcpy(pad, input + offset, remaining);
    pad[remaining] = 0x06; // SHA3 domain separator
    pad[rate - 1] |= 0x80;
    for (size_t i = 0; i < rate/8; i++)
        state[i] ^= ((uint64_t*)pad)[i];
    keccak_f1600(state);
    
    // Squeeze (32 bytes for SHA3-256)
    memcpy(output, state, 32);
}

// 64x64 HeavyHash matrix — initialized from genesis block hash
// Each element is a 4-bit nibble (0-15)
class HeavyMatrix {
public:
    uint8_t matrix[64][64];
    
    HeavyMatrix() {
        // Initialize with deterministic values from BitVault genesis
        // In production, this would be derived from the genesis block
        uint8_t seed[32];
        const char* genesis = "759dd00203505cf662344765ed3d503b";
        for (int i = 0; i < 32; i++) {
            char hex[3] = {genesis[i % 32], genesis[(i+1) % 32], 0};
            seed[i] = (uint8_t)(strtol(hex, nullptr, 16) & 0xFF);
        }
        
        // Generate matrix using seed expansion
        uint8_t expanded[32];
        memcpy(expanded, seed, 32);
        int idx = 0;
        for (int row = 0; row < 64; row++) {
            for (int col = 0; col < 64; col++) {
                if (idx % 32 == 0) {
                    sha3_256(expanded, 32, expanded);
                }
                // Each matrix element is a nibble (0-15)
                matrix[row][col] = expanded[idx % 32] & 0x0F;
                idx++;
            }
        }
    }
    
    // Ensure matrix rank is sufficient (non-degenerate)
    bool isValid() const {
        // Simple check: no all-zero rows
        for (int i = 0; i < 64; i++) {
            bool allZero = true;
            for (int j = 0; j < 64; j++) {
                if (matrix[i][j] != 0) { allZero = false; break; }
            }
            if (allZero) return false;
        }
        return true;
    }
};

// Global matrix instance
static HeavyMatrix HEAVY_MATRIX;

// Convert 32-byte hash to 64 nibbles for matrix input
inline void hashToNibbles(const uint8_t* hash, uint8_t nibbles[64]) {
    for (int i = 0; i < 32; i++) {
        nibbles[2*i]     = (hash[i] >> 4) & 0x0F;
        nibbles[2*i + 1] =  hash[i]       & 0x0F;
    }
}

// Convert 64 nibbles back to 32-byte hash
inline void nibblesToHash(const uint8_t nibbles[64], uint8_t* hash) {
    for (int i = 0; i < 32; i++) {
        hash[i] = (nibbles[2*i] << 4) | (nibbles[2*i + 1] & 0x0F);
    }
}

// Matrix-vector multiplication in GF(16)
inline void matrixMultiply(const uint8_t matrix[64][64], const uint8_t input[64], uint8_t output[64]) {
    for (int i = 0; i < 64; i++) {
        uint16_t sum = 0;
        for (int j = 0; j < 64; j++) {
            sum += (uint16_t)matrix[i][j] * (uint16_t)input[j];
        }
        // Reduce to nibble
        output[i] = (uint8_t)((sum >> 10) ^ ((sum >> 5) & 0x1F) ^ (sum & 0x1F)) & 0x0F;
    }
}

/// Main KHeavyHash function
/// @param header Block header bytes
/// @param len Header length
/// @param output 32-byte output hash
inline void compute(const uint8_t* header, size_t len, uint8_t* output) {
    // Step 1: SHA3-256 of block header
    uint8_t sha3_hash[32];
    sha3_256(header, len, sha3_hash);
    
    // Step 2: Convert to nibbles
    uint8_t input_nibbles[64];
    hashToNibbles(sha3_hash, input_nibbles);
    
    // Step 3: Matrix multiplication
    uint8_t result_nibbles[64];
    matrixMultiply(HEAVY_MATRIX.matrix, input_nibbles, result_nibbles);
    
    // Step 4: Convert back to bytes
    uint8_t matrix_hash[32];
    nibblesToHash(result_nibbles, matrix_hash);
    
    // Step 5: XOR with original SHA3 hash
    for (int i = 0; i < 32; i++) {
        matrix_hash[i] ^= sha3_hash[i];
    }
    
    // Step 6: Final SHA3-256
    sha3_256(matrix_hash, 32, output);
}

/// Verify a KHeavyHash result
inline bool verify(const uint8_t* header, size_t len, const uint8_t* expected) {
    uint8_t result[32];
    compute(header, len, result);
    return memcmp(result, expected, 32) == 0;
}

} // namespace kheavyhash

#endif // KHEAVYHASH_H

#include <uint256.h>
#include <span.h>
uint256 HashKHeavyHash(Span<const unsigned char> input);
