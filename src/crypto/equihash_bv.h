#ifndef BITVAULT_CRYPTO_EQUIHASH_BV_H
#define BITVAULT_CRYPTO_EQUIHASH_BV_H
#include <uint256.h>
#include <span.h>
uint256 HashEquihashBV(Span<const unsigned char> input);
#endif
