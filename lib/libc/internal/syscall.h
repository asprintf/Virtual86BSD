/* Internal: system call interface.
 *
 * __syscallN() return the raw kernel result (-errno on failure);
 * __syscall_ret() converts that to the C convention (-1 and errno).
 * With gcc/clang the calls are inlined; otherwise (c17) they go through
 * the assembly function __syscall6 in arch/x86_64/syscall.s.
 */
#ifndef _INTERNAL_SYSCALL_H
#define _INTERNAL_SYSCALL_H
#include <syscall_nr.h>

long __syscall6(long, long, long, long, long, long, long);
long __syscall_ret(unsigned long);

#if defined(__GNUC__) || defined(__clang__)
static inline long __sc(long n, long a, long b, long c, long d, long e, long f)
{
	register long r10 __asm__("r10") = d;
	register long r8 __asm__("r8") = e;
	register long r9 __asm__("r9") = f;
	long ret;
	__asm__ __volatile__ ("syscall" : "=a"(ret)
		: "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
		: "rcx", "r11", "memory");
	return ret;
}
#else
#define __sc __syscall6

#endif


#define __syscall0(n)             __sc(n, 0, 0, 0, 0, 0, 0)
#define __syscall1(n,a)           __sc(n, (long)(a), 0, 0, 0, 0, 0)
#define __syscall2(n,a,b)         __sc(n, (long)(a), (long)(b), 0, 0, 0, 0)
#define __syscall3(n,a,b,c)       __sc(n, (long)(a), (long)(b), (long)(c), 0, 0, 0)
#define __syscall4(n,a,b,c,d)     __sc(n, (long)(a), (long)(b), (long)(c), (long)(d), 0, 0)
#define __syscall5(n,a,b,c,d,e)   __sc(n, (long)(a), (long)(b), (long)(c), (long)(d), (long)(e), 0)
#define __syscall6x(n,a,b,c,d,e,f) __sc(n, (long)(a), (long)(b), (long)(c), (long)(d), (long)(e), (long)(f))

#define __sys0(n)             __syscall_ret(__syscall0(n))
#define __sys1(n,a)           __syscall_ret(__syscall1(n,a))
#define __sys2(n,a,b)         __syscall_ret(__syscall2(n,a,b))
#define __sys3(n,a,b,c)       __syscall_ret(__syscall3(n,a,b,c))
#define __sys4(n,a,b,c,d)     __syscall_ret(__syscall4(n,a,b,c,d))
#define __sys5(n,a,b,c,d,e)   __syscall_ret(__syscall5(n,a,b,c,d,e))
#define __sys6(n,a,b,c,d,e,f) __syscall_ret(__syscall6x(n,a,b,c,d,e,f))



/* System calls that are cancellation points (src/thread/__syscall_cp.c). */
long __syscall_cp_c(long, long, long, long, long, long, long);
#define __CPA(x) ((long)(x))
#define __syscall_cp1(n,a)           __syscall_cp_c(n,__CPA(a),0,0,0,0,0)
#define __syscall_cp2(n,a,b)         __syscall_cp_c(n,__CPA(a),__CPA(b),0,0,0,0)
#define __syscall_cp3(n,a,b,c)       __syscall_cp_c(n,__CPA(a),__CPA(b),__CPA(c),0,0,0)
#define __syscall_cp4(n,a,b,c,d)     __syscall_cp_c(n,__CPA(a),__CPA(b),__CPA(c),__CPA(d),0,0)
#define __syscall_cp5(n,a,b,c,d,e)   __syscall_cp_c(n,__CPA(a),__CPA(b),__CPA(c),__CPA(d),__CPA(e),0)
#define __syscall_cp6(n,a,b,c,d,e,f) __syscall_cp_c(n,__CPA(a),__CPA(b),__CPA(c),__CPA(d),__CPA(e),__CPA(f))
#define __sys_cp1(n,a)           __syscall_ret((unsigned long)__syscall_cp1(n,a))
#define __sys_cp2(n,a,b)         __syscall_ret((unsigned long)__syscall_cp2(n,a,b))
#define __sys_cp3(n,a,b,c)       __syscall_ret((unsigned long)__syscall_cp3(n,a,b,c))
#define __sys_cp4(n,a,b,c,d)     __syscall_ret((unsigned long)__syscall_cp4(n,a,b,c,d))
#define __sys_cp5(n,a,b,c,d,e)   __syscall_ret((unsigned long)__syscall_cp5(n,a,b,c,d,e))
#define __sys_cp6(n,a,b,c,d,e,f) __syscall_ret((unsigned long)__syscall_cp6(n,a,b,c,d,e,f))


#endif
