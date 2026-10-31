#ifndef _SETJMP_H
#define _SETJMP_H
#include <bits/features.h>

/* Layout as glibc's on x86-64: registers, a flag, the signal mask. */
typedef struct __jmp_buf_tag {
	unsigned long __jb[8];
	int __fl;
	unsigned long __ss[16];
} jmp_buf[1];

#if __POSIX_VISIBLE
typedef jmp_buf sigjmp_buf;
#endif

#if defined(__GNUC__) || defined(__clang__)
#define __RETURNS_TWICE __attribute__((__returns_twice__))
#else
#define __RETURNS_TWICE
#endif

__BEGIN_DECLS
int setjmp(jmp_buf) __RETURNS_TWICE;
__NORETURN void longjmp(jmp_buf, int);
#if __POSIX_VISIBLE
int sigsetjmp(sigjmp_buf, int) __RETURNS_TWICE;
__NORETURN void siglongjmp(sigjmp_buf, int);
#endif
__END_DECLS
#define setjmp setjmp
#endif
