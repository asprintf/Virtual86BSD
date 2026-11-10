/* Internal: DES for crypt(), encrypt() and setkey(). */
#ifndef _CRYPT_DES_H
#define _CRYPT_DES_H
#include <stdint.h>

struct __des_key {
	uint64_t k[16];            /* 48-bit subkeys */
};

void __des_setkey(struct __des_key *, uint64_t key);
/* encrypt (dec 0) or decrypt a block; salt: E-box bits i and i+24 are
 * swapped for each bit i (0 = the first) set in the 24-bit salt */
uint64_t __des_crypt(const struct __des_key *, uint64_t block, uint32_t salt, int dec);

#endif
