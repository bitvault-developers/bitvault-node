#ifndef BITVAULT_CRYPTO_VERSAHASH_H
#define BITVAULT_CRYPTO_VERSAHASH_H

#include <uint256.h>
#include <span.h>

/** VersaHash - BitVault custom algorithm combining SHA256d + Blake + Groestl */
uint256 HashVersaHash(Span<const unsigned char> input);

#endif
