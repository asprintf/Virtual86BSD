/* setkey and encrypt (XSI): DES on arrays of 64 bytes holding one bit
 * each.  The key is kept here; crypt() does not disturb it. */
#include <stdlib.h>
#include <unistd.h>
#include "des.h"

static struct __des_key key;

static uint64_t pack(const char *b)
{
	uint64_t r = 0;
	int i;
	for (i = 0; i < 64; i++)
		r = r << 1 | (b[i] & 1);
	return r;
}

void setkey(const char *k)
{
	__des_setkey(&key, pack(k));
}

void encrypt(char block[64], int edflag)
{
	uint64_t r = __des_crypt(&key, pack(block), 0, edflag != 0);
	int i;
	for (i = 0; i < 64; i++)
		block[i] = (char)(r >> (63 - i) & 1);
}
