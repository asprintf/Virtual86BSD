/* Internal: exact binary-to-decimal conversion for printf. */
#ifndef _INTERNAL_FPDIGITS_H
#define _INTERNAL_FPDIGITS_H
#include <stdint.h>

#define FP_CLS_ZERO   0
#define FP_CLS_FINITE 1
#define FP_CLS_INF    2
#define FP_CLS_NAN    3

/* the most digits __fp_digits can produce, plus room */
#define FP_DIGITS_MAX 11600

void __ld_parts(long double x, uint64_t *m, int *e, int *sign, int *cls);
/* m * 2^e exactly as 0.digits * 10^dexp; returns the number of digits */
int __fp_digits(uint64_t m, int e, char *digits, int *dexp);
#endif
