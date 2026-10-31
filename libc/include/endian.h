#ifndef _ENDIAN_H
#define _ENDIAN_H
#include <bits/features.h>
#define __NEED_uint16_t
#define __NEED_uint32_t
#define __NEED_uint64_t
#include <bits/alltypes.h>

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN    4321
#define PDP_ENDIAN    3412
#define BYTE_ORDER    LITTLE_ENDIAN

static __INLINE uint16_t __bswap16(uint16_t __x)
{ return (uint16_t)(__x << 8 | __x >> 8); }
static __INLINE uint32_t __bswap32(uint32_t __x)
{ return __x >> 24 | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000) | __x << 24; }
static __INLINE uint64_t __bswap64(uint64_t __x)
{ return (uint64_t)__bswap32((uint32_t)__x) << 32 | __bswap32((uint32_t)(__x >> 32)); }

#define htobe16(x) __bswap16(x)
#define be16toh(x) __bswap16(x)
#define htobe32(x) __bswap32(x)
#define be32toh(x) __bswap32(x)
#define htobe64(x) __bswap64(x)
#define be64toh(x) __bswap64(x)
#define htole16(x) (uint16_t)(x)
#define le16toh(x) (uint16_t)(x)
#define htole32(x) (uint32_t)(x)
#define le32toh(x) (uint32_t)(x)
#define htole64(x) (uint64_t)(x)
#define le64toh(x) (uint64_t)(x)
#endif
