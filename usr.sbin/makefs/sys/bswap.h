/* V86BSD: the byte swap functions of NetBSD's <sys/bswap.h> that ufs_bswap.h uses */
#ifndef _SYS_BSWAP_H_
#define _SYS_BSWAP_H_
#include <stdint.h>

static inline uint16_t
bswap16(uint16_t x)
{
	return (uint16_t)((x << 8) | (x >> 8));
}

static inline uint32_t
bswap32(uint32_t x)
{
	return (x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | (x >> 24);
}

static inline uint64_t
bswap64(uint64_t x)
{
	return ((uint64_t)bswap32((uint32_t)x) << 32) | bswap32((uint32_t)(x >> 32));
}

#endif
