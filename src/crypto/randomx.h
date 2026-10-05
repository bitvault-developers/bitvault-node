#ifndef BITVAULT_CRYPTO_RANDOMX_H
#define BITVAULT_CRYPTO_RANDOMX_H

#include <uint256.h>
#include <vector>

uint256 RandomXHash(const std::vector<unsigned char>& input);

#endif
