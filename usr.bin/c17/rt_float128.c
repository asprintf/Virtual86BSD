/* rt_float128.c - c17: run-time support: binary128 (_Float128, gcc's
   __float128) arithmetic and comparison, as the C libraries' static
   archives use it (glibc's libc.a formats and converts _Float128).
 *
 * The functions of the ABI (__addtf3 and the others) pass their operands
 * in %xmm registers, which C17 cannot express; rt_float128.s has them,
 * and calls the functions here with pointers to the 16-byte values (low
 * 64 bits first).  Results are rounded to nearest, ties to even; the
 * rounding mode and the exception flags of the floating-point environment
 * are neither consulted nor set.  A NaN result is the quiet form of a NaN
 * operand, else the default NaN.
 */
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint64_t hi, lo;
} U128;

static U128 u128(uint64_t hi, uint64_t lo) {
  U128 r;
  r.hi = hi;
  r.lo = lo;
  return r;
}

static U128 add128(U128 a, U128 b) {
  U128 r;
  r.lo = a.lo + b.lo;
  r.hi = a.hi + b.hi + (r.lo < a.lo);
  return r;
}

static U128 sub128(U128 a, U128 b) {
  U128 r;
  r.lo = a.lo - b.lo;
  r.hi = a.hi - b.hi - (a.lo < b.lo);
  return r;
}

static int cmp128(U128 a, U128 b) {
  if (a.hi != b.hi)
    return a.hi < b.hi ? -1 : 1;
  return a.lo < b.lo ? -1 : a.lo > b.lo;
}

static int is_zero128(U128 a) { return !a.hi && !a.lo; }

static U128 shl128(U128 a, int n) {
  if (n <= 0)
    return a;
  if (n >= 128)
    return u128(0, 0);
  if (n >= 64)
    return u128(a.lo << (n - 64), 0);
  return u128(a.hi << n | a.lo >> (64 - n), a.lo << n);
}

static U128 shr128(U128 a, int n) {
  if (n <= 0)
    return a;
  if (n >= 128)
    return u128(0, 0);
  if (n >= 64)
    return u128(0, a.hi >> (n - 64));
  return u128(a.hi >> n, a.lo >> n | a.hi << (64 - n));
}

/* shift right, ORing the bits shifted out into the lowest bit (sticky) */
static U128 shr_sticky(U128 a, int n) {
  if (n <= 0)
    return a;
  U128 r = shr128(a, n);
  if (cmp128(shl128(r, n), a) != 0 || n >= 128)
    r.lo |= is_zero128(a) ? 0 : 1;
  return r;
}

static int clz128(U128 a) {
  int n = 0;
  if (!a.hi) {
    n = 64;
    a.hi = a.lo;
    a.lo = 0;
    if (!a.hi)
      return 128;
  }
  while (!(a.hi >> 63)) {
    a.hi <<= 1;
    n++;
  }
  return n;
}

static void mul64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo) {
  uint64_t a0 = a & 0xffffffff, a1 = a >> 32, b0 = b & 0xffffffff, b1 = b >> 32;
  uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  uint64_t mid = (p00 >> 32) + (p01 & 0xffffffff) + (p10 & 0xffffffff);
  *lo = (p00 & 0xffffffff) | mid << 32;
  *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

#define EXP_MAX 0x7fff
#define BIAS 16383
#define QNAN_HI 0x7fff800000000000ULL

typedef struct {
  int sign;
  int exp;     /* biased; 0 for zero and subnormals (adjusted) */
  U128 sig;    /* with the implicit bit at 112 for normal numbers */
  int cls;     /* 0 zero, 1 finite, 2 infinity, 3 NaN */
} Unpacked;

static Unpacked unpack(const uint64_t *v) {
  Unpacked u;
  uint64_t lo = v[0], hi = v[1];
  u.sign = hi >> 63;
  u.exp = (hi >> 48) & EXP_MAX;
  u.sig = u128(hi & 0xffffffffffffULL, lo);
  if (u.exp == EXP_MAX) {
    u.cls = is_zero128(u.sig) ? 2 : 3;
  } else if (u.exp == 0) {
    if (is_zero128(u.sig)) {
      u.cls = 0;
    } else {  /* subnormal: normalize */
      u.cls = 1;
      int sh = clz128(u.sig) - 15;
      u.sig = shl128(u.sig, sh);
      u.exp = 1 - sh;
    }
  } else {
    u.cls = 1;
    u.sig.hi |= 1ULL << 48;
  }
  return u;
}

static void pack(uint64_t *r, int sign, int exp, U128 sig) {
  r[0] = sig.lo;
  r[1] = (uint64_t)sign << 63 | (uint64_t)exp << 48 | (sig.hi & 0xffffffffffffULL);
}

static void make_nan(uint64_t *r, const uint64_t *a, const uint64_t *b) {
  const uint64_t *src = NULL;
  if (a && ((a[1] >> 48) & EXP_MAX) == EXP_MAX && ((a[1] & 0xffffffffffffULL) || a[0]))
    src = a;
  else if (b && ((b[1] >> 48) & EXP_MAX) == EXP_MAX && ((b[1] & 0xffffffffffffULL) || b[0]))
    src = b;
  if (src) {
    r[0] = src[0];
    r[1] = src[1] | 0x0000800000000000ULL;
  } else {
    r[0] = 0;
    r[1] = QNAN_HI;
  }
}

/* sig has its leading bit at 115 (three bits below the 112 of the result:
   guard, round, sticky) for a number of biased exponent exp; rounds and
   packs, with overflow to infinity and gradual underflow */
static void round_pack(uint64_t *r, int sign, int exp, U128 sig) {
  if (is_zero128(sig)) {
    pack(r, sign, 0, u128(0, 0));
    return;
  }
  /* normalize so that the leading bit is at 115 */
  int lead = 127 - clz128(sig);
  if (lead > 115) {
    sig = shr_sticky(sig, lead - 115);
    exp += lead - 115;
  } else if (lead < 115) {
    sig = shl128(sig, 115 - lead);
    exp -= 115 - lead;
  }
  if (exp <= 0) {  /* subnormal */
    sig = shr_sticky(sig, 1 - exp);
    exp = 0;
  }
  unsigned rest = sig.lo & 7;
  sig = shr128(sig, 3);
  if (rest > 4 || (rest == 4 && (sig.lo & 1)))
    sig = add128(sig, u128(0, 1));
  if (exp == 0) {
    if (sig.hi >> 48)  /* rounded up to the smallest normal */
      exp = 1;
  } else if (sig.hi >> 49) {  /* rounding carried out */
    sig = shr128(sig, 1);
    exp++;
  }
  if (exp >= EXP_MAX) {
    pack(r, sign, EXP_MAX, u128(0, 0));
    return;
  }
  pack(r, sign, exp, sig);
}

static void addsub(uint64_t *r, const uint64_t *a, const uint64_t *b, int negate_b) {
  Unpacked x = unpack(a), y = unpack(b);
  y.sign ^= negate_b;
  if (x.cls == 3 || y.cls == 3) {
    make_nan(r, a, b);
    return;
  }
  if (x.cls == 2 || y.cls == 2) {
    if (x.cls == 2 && y.cls == 2 && x.sign != y.sign) {
      make_nan(r, NULL, NULL);
      return;
    }
    pack(r, x.cls == 2 ? x.sign : y.sign, EXP_MAX, u128(0, 0));
    return;
  }
  if (x.cls == 0 && y.cls == 0) {
    pack(r, x.sign & y.sign, 0, u128(0, 0));
    return;
  }
  if (x.cls == 0) {
    round_pack(r, y.sign, y.exp, shl128(y.sig, 3));
    return;
  }
  if (y.cls == 0) {
    round_pack(r, x.sign, x.exp, shl128(x.sig, 3));
    return;
  }
  U128 xs = shl128(x.sig, 3), ys = shl128(y.sig, 3);
  int exp = x.exp;
  if (x.exp > y.exp) {
    ys = shr_sticky(ys, x.exp - y.exp);
  } else if (y.exp > x.exp) {
    xs = shr_sticky(xs, y.exp - x.exp);
    exp = y.exp;
  }
  if (x.sign == y.sign) {
    round_pack(r, x.sign, exp, add128(xs, ys));
    return;
  }
  int c = cmp128(xs, ys);
  if (c == 0) {
    pack(r, 0, 0, u128(0, 0));  /* +0 when rounding to nearest */
    return;
  }
  if (c > 0)
    round_pack(r, x.sign, exp, sub128(xs, ys));
  else
    round_pack(r, y.sign, exp, sub128(ys, xs));
}

void __c17_addtf3(uint64_t *r, const uint64_t *a, const uint64_t *b) { addsub(r, a, b, 0); }
void __c17_subtf3(uint64_t *r, const uint64_t *a, const uint64_t *b) { addsub(r, a, b, 1); }

void __c17_multf3(uint64_t *r, const uint64_t *a, const uint64_t *b) {
  Unpacked x = unpack(a), y = unpack(b);
  int sign = x.sign ^ y.sign;
  if (x.cls == 3 || y.cls == 3) {
    make_nan(r, a, b);
    return;
  }
  if (x.cls == 2 || y.cls == 2) {
    if (x.cls == 0 || y.cls == 0)
      make_nan(r, NULL, NULL);
    else
      pack(r, sign, EXP_MAX, u128(0, 0));
    return;
  }
  if (x.cls == 0 || y.cls == 0) {
    pack(r, sign, 0, u128(0, 0));
    return;
  }
  /* 113 x 113 bits: a 226-bit product in four words */
  uint64_t p[4] = {0, 0, 0, 0}, h, l;
  uint64_t xa[2] = {x.sig.lo, x.sig.hi}, ya[2] = {y.sig.lo, y.sig.hi};
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++) {
      mul64(xa[i], ya[j], &h, &l);
      int k = i + j;
      uint64_t old = p[k];
      p[k] += l;
      uint64_t carry = p[k] < old;
      old = p[k + 1];
      p[k + 1] += h + carry;
      carry = p[k + 1] < old || (carry && p[k + 1] == old);
      for (int m = k + 2; carry && m < 4; m++) {
        p[m]++;
        carry = p[m] == 0;
      }
    }
  /* the product's leading bit is at 224 or 225 (96 or 97 within the top
     two words): take 116 bits from there, the rest as sticky */
  U128 hi = u128(p[3], p[2]);
  int lead = 127 - clz128(hi);
  int sh = 115 - lead;  /* 18 or 19 */
  U128 sig = shl128(hi, sh);
  sig.lo |= p[1] >> (64 - sh);
  if ((p[1] << sh) || p[0])
    sig.lo |= 1;
  /* sig is the product shifted right by 128 - sh bits, of 2^-224 units */
  int exp = x.exp + y.exp - BIAS + (lead + 128 - 224);
  round_pack(r, sign, exp, sig);
}

void __c17_divtf3(uint64_t *r, const uint64_t *a, const uint64_t *b) {
  Unpacked x = unpack(a), y = unpack(b);
  int sign = x.sign ^ y.sign;
  if (x.cls == 3 || y.cls == 3) {
    make_nan(r, a, b);
    return;
  }
  if (x.cls == 2) {
    if (y.cls == 2)
      make_nan(r, NULL, NULL);
    else
      pack(r, sign, EXP_MAX, u128(0, 0));
    return;
  }
  if (y.cls == 2) {
    pack(r, sign, 0, u128(0, 0));
    return;
  }
  if (y.cls == 0) {
    if (x.cls == 0)
      make_nan(r, NULL, NULL);
    else
      pack(r, sign, EXP_MAX, u128(0, 0));
    return;
  }
  if (x.cls == 0) {
    pack(r, sign, 0, u128(0, 0));
    return;
  }
  /* quotient bits by restoring division: 116 bits, then the remainder as
     sticky */
  U128 rem = x.sig, d = y.sig, q = u128(0, 0);
  int exp = x.exp - y.exp + BIAS;
  if (cmp128(rem, d) < 0) {
    rem = shl128(rem, 1);
    exp--;
  }
  for (int i = 0; i < 116; i++) {
    q = shl128(q, 1);
    if (cmp128(rem, d) >= 0) {
      rem = sub128(rem, d);
      q.lo |= 1;
    }
    rem = shl128(rem, 1);
  }
  if (!is_zero128(rem))
    q.lo |= 1;
  round_pack(r, sign, exp, q);
}

/* comparisons: -1, 0, 1, or 2 when unordered */
int __c17_cmptf2(const uint64_t *a, const uint64_t *b) {
  Unpacked x = unpack(a), y = unpack(b);
  if (x.cls == 3 || y.cls == 3)
    return 2;
  if (x.cls == 0 && y.cls == 0)
    return 0;
  if (x.sign != y.sign)
    return x.sign ? -1 : 1;
  int c;
  uint64_t ah = a[1] & 0x7fffffffffffffffULL, bh = b[1] & 0x7fffffffffffffffULL;
  if (ah != bh)
    c = ah < bh ? -1 : 1;
  else
    c = a[0] < b[0] ? -1 : a[0] > b[0];
  return x.sign ? -c : c;
}
