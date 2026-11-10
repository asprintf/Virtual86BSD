/* types.h - the system's types, built on the machine's (<machine/_types.h>). */
#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include <stddef.h>
#include <machine/_types.h>

typedef __int8_t int8_t;
typedef __uint8_t uint8_t;
typedef __int16_t int16_t;
typedef __uint16_t uint16_t;
typedef __int32_t int32_t;
typedef __uint32_t uint32_t;
typedef __int64_t int64_t;
typedef __uint64_t uint64_t;
typedef __intptr_t intptr_t;
typedef __uintptr_t uintptr_t;
typedef __ssize_t ssize_t;
typedef __off_t off_t;
typedef __pid_t pid_t;
typedef __uid_t uid_t;
typedef __gid_t gid_t;
typedef __mode_t mode_t;
typedef __dev_t dev_t;
typedef __ino_t ino_t;

#endif
