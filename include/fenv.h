#ifndef _FENV_H
#define _FENV_H
#include <bits/features.h>

/* The floating-point environment of x86-64: both the x87 unit (long
 * double) and SSE (float, double), kept in step by these functions.
 * The layout and values are those of the System V ABI (as glibc's). */
#define FE_INVALID    0x01
#define FE_DIVBYZERO  0x04
#define FE_OVERFLOW   0x08
#define FE_UNDERFLOW  0x10
#define FE_INEXACT    0x20
#define FE_ALL_EXCEPT 0x3d

#define FE_TONEAREST  0
#define FE_DOWNWARD   0x400
#define FE_UPWARD     0x800
#define FE_TOWARDZERO 0xc00

typedef unsigned short fexcept_t;

/* the x87 environment (fnstenv) followed by MXCSR */
typedef struct {
	unsigned short __control_word, __reserved1, __status_word, __reserved2;
	unsigned short __tags, __reserved3;
	unsigned int __eip;
	unsigned short __cs_selector, __opcode;
	unsigned int __data_offset;
	unsigned short __data_selector, __reserved5;
	unsigned int __mxcsr;
} fenv_t;

#define FE_DFL_ENV ((const fenv_t *)-1)

__BEGIN_DECLS
int feclearexcept(int);
int fegetenv(fenv_t *);
int fegetexceptflag(fexcept_t *, int);
int fegetround(void);
int feholdexcept(fenv_t *);
int feraiseexcept(int);
int fesetenv(const fenv_t *);
int fesetexceptflag(const fexcept_t *, int);
int fesetround(int);
int fetestexcept(int);
int feupdateenv(const fenv_t *);
__END_DECLS

#endif
