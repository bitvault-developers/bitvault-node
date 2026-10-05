#include <cstdio>
#include <cstring>
#include "crypto/kheavyhash.h"

int main() {
    // Test 1: Basic hash computation
    const char* test_header = "BitVault KHeavyHash Test Block Header 12345";
    uint8_t output[32];
    
    kheavyhash::compute((const uint8_t*)test_header, strlen(test_header), output);
    
    printf("KHeavyHash Test Results:\n");
    printf("Input:  \"%s\"\n", test_header);
    printf("Output: ");
    for (int i = 0; i < 32; i++) printf("%02x", output[i]);
    printf("\n");
    
    // Test 2: Verify produces same result
    bool verified = kheavyhash::verify((const uint8_t*)test_header, strlen(test_header), output);
    printf("Verify: %s\n", verified ? "PASS" : "FAIL");
    
    // Test 3: Different input produces different hash
    const char* test2 = "Different header data";
    uint8_t output2[32];
    kheavyhash::compute((const uint8_t*)test2, strlen(test2), output2);
    
    bool different = (memcmp(output, output2, 32) != 0);
    printf("Unique: %s\n", different ? "PASS" : "FAIL");
    
    // Test 4: Empty input
    uint8_t output3[32];
    kheavyhash::compute((const uint8_t*)"", 0, output3);
    printf("Empty:  ");
    for (int i = 0; i < 32; i++) printf("%02x", output3[i]);
    printf("\n");
    
    // Test 5: Matrix validity
    printf("Matrix: %s\n", kheavyhash::HEAVY_MATRIX.isValid() ? "VALID" : "INVALID");
    
    // Test 6: Deterministic (same input = same output)
    uint8_t output4[32];
    kheavyhash::compute((const uint8_t*)test_header, strlen(test_header), output4);
    bool deterministic = (memcmp(output, output4, 32) == 0);
    printf("Determ: %s\n", deterministic ? "PASS" : "FAIL");
    
    printf("\nAll tests: %s\n", (verified && different && deterministic) ? "PASSED ✅" : "FAILED ❌");
    return (verified && different && deterministic) ? 0 : 1;
}
