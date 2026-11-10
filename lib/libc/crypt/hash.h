/* Internal: MD5, SHA-256 and SHA-512 for crypt(). */
#ifndef _CRYPT_HASH_H
#define _CRYPT_HASH_H
#include <stddef.h>
#include <stdint.h>

struct __hash {
	unsigned char buf[128];
	uint64_t len;              /* bytes so far */
	unsigned bs;               /* block size: 64 or 128 */
	unsigned outlen;
	void (*block)(struct __hash *, const unsigned char *);
	union {
		uint32_t w[8];
		uint64_t d[8];
	} s;
};

enum { __MD5, __SHA256, __SHA512 };

void __hash_init(struct __hash *, int alg);
void __hash_update(struct __hash *, const void *, size_t);
void __hash_final(struct __hash *, unsigned char *out);   /* outlen bytes */

#endif
