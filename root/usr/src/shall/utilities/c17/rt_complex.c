/* rt_complex.c - c17: run-time support: multiplication
   and division of complex numbers (C17 Annex G, G.5.1), under the names
   and calling convention of the x86-64 System V ABI (__mulsc3,
   __muldc3, __mulxc3, __divsc3, __divdc3, __divxc3).
 *
 * The algorithms are those of the examples in G.5.1: the products and
 * quotients are first computed the straightforward way (division with
 * scaling by a power of two to avoid overflow), and only a NaN+NaNi
 * result is examined further, to recover infinities and zeros.
 *
 * float operands are computed in double and the result rounded once.
 *
 * This file uses no library functions, so it can be linked with any C
 * library.  The sign bit of each floating type is found in its object
 * representation: x86-64 is little-endian and the sign is the top bit
 * of the last byte (double byte 7, long double byte 9).
 */

#define DEFINE(SUF, T, SIGNBYTE)                                                     \
  typedef union {                                                                    \
    T v;                                                                             \
    unsigned char b[sizeof(T)];                                                      \
  } bits_##SUF;                                                                      \
  typedef union {                                                                    \
    T _Complex z;                                                                    \
    T p[2];                                                                          \
  } cplx_##SUF;                                                                      \
                                                                                     \
  static int isnan_##SUF(T x) { return x != x; }                                    \
  static int isinf_##SUF(T x) { return x == x && x - x != x - x; }                  \
  static int isfinite_##SUF(T x) { return x - x == x - x; }                         \
  static int signbit_##SUF(T x) {                                                    \
    bits_##SUF u;                                                                    \
    u.v = x;                                                                         \
    return (u.b[SIGNBYTE] & 0x80) != 0;                                              \
  }                                                                                  \
  static T copysign_##SUF(T mag, T sgn) {                                            \
    bits_##SUF u;                                                                    \
    u.v = mag;                                                                       \
    u.b[SIGNBYTE] = (u.b[SIGNBYTE] & 0x7f) | (signbit_##SUF(sgn) ? 0x80 : 0);         \
    return u.v;                                                                      \
  }                                                                                  \
  static T fabs_##SUF(T x) { return copysign_##SUF(x, 1); }                         \
  static T inf_##SUF(void) {                                                         \
    T x = 0x1p60;                                                                    \
    for (;;) {                                                                       \
      T y = x * x;                                                                   \
      if (y == x)                                                                    \
        return x;                                                                    \
      x = y;                                                                         \
    }                                                                                \
  }                                                                                  \
  /* floor(log2(x)) for finite x > 0; exact, since only powers of 2 are used */      \
  static int ilogb_##SUF(T x) {                                                      \
    int e = 0;                                                                       \
    while (x >= 0x1p60) {                                                            \
      x *= 0x1p-60;                                                                  \
      e += 60;                                                                       \
    }                                                                                \
    while (x < 1) {                                                                  \
      x *= 0x1p60;                                                                   \
      e -= 60;                                                                       \
    }                                                                                \
    while (x >= 2) {                                                                 \
      x *= 0.5;                                                                      \
      e++;                                                                           \
    }                                                                                \
    return e;                                                                        \
  }                                                                                  \
  static T scalbn_##SUF(T x, int n) {                                                \
    while (n > 60) {                                                                 \
      x *= 0x1p60;                                                                   \
      n -= 60;                                                                       \
    }                                                                                \
    while (n < -60) {                                                                \
      x *= 0x1p-60;                                                                  \
      n += 60;                                                                       \
    }                                                                                \
    T p = (T)(1ULL << (n < 0 ? -n : n));                                             \
    return n < 0 ? x / p : x * p;                                                    \
  }                                                                                  \
                                                                                     \
  T _Complex __mul##SUF##c3(T a, T b, T c, T d) {                                    \
    T ac = a * c, bd = b * d, ad = a * d, bc = b * c;                                \
    cplx_##SUF r;                                                                    \
    r.p[0] = ac - bd;                                                                \
    r.p[1] = ad + bc;                                                                \
    if (isnan_##SUF(r.p[0]) && isnan_##SUF(r.p[1])) {                                \
      int recalc = 0;                                                                \
      if (isinf_##SUF(a) || isinf_##SUF(b)) {                                        \
        a = copysign_##SUF(isinf_##SUF(a) ? 1 : 0, a);                               \
        b = copysign_##SUF(isinf_##SUF(b) ? 1 : 0, b);                               \
        if (isnan_##SUF(c))                                                          \
          c = copysign_##SUF(0, c);                                                  \
        if (isnan_##SUF(d))                                                          \
          d = copysign_##SUF(0, d);                                                  \
        recalc = 1;                                                                  \
      }                                                                              \
      if (isinf_##SUF(c) || isinf_##SUF(d)) {                                        \
        c = copysign_##SUF(isinf_##SUF(c) ? 1 : 0, c);                               \
        d = copysign_##SUF(isinf_##SUF(d) ? 1 : 0, d);                               \
        if (isnan_##SUF(a))                                                          \
          a = copysign_##SUF(0, a);                                                  \
        if (isnan_##SUF(b))                                                          \
          b = copysign_##SUF(0, b);                                                  \
        recalc = 1;                                                                  \
      }                                                                              \
      if (!recalc && (isinf_##SUF(ac) || isinf_##SUF(bd) || isinf_##SUF(ad) ||       \
                      isinf_##SUF(bc))) {                                            \
        if (isnan_##SUF(a))                                                          \
          a = copysign_##SUF(0, a);                                                  \
        if (isnan_##SUF(b))                                                          \
          b = copysign_##SUF(0, b);                                                  \
        if (isnan_##SUF(c))                                                          \
          c = copysign_##SUF(0, c);                                                  \
        if (isnan_##SUF(d))                                                          \
          d = copysign_##SUF(0, d);                                                  \
        recalc = 1;                                                                  \
      }                                                                              \
      if (recalc) {                                                                  \
        T inf = inf_##SUF();                                                         \
        r.p[0] = inf * (a * c - b * d);                                              \
        r.p[1] = inf * (a * d + b * c);                                              \
      }                                                                              \
    }                                                                                \
    return r.z;                                                                      \
  }                                                                                  \
                                                                                     \
  T _Complex __div##SUF##c3(T a, T b, T c, T d) {                                    \
    T inf = inf_##SUF();                                                             \
    T fc = fabs_##SUF(c), fd = fabs_##SUF(d);                                        \
    T m = isnan_##SUF(fc) ? fd : isnan_##SUF(fd) ? fc : fc > fd ? fc : fd;           \
    int ilogbw = 0;                                                                  \
    int logbw_inf = isinf_##SUF(m);                                                  \
    if (isfinite_##SUF(m) && m != 0) {                                               \
      ilogbw = ilogb_##SUF(m);                                                       \
      c = scalbn_##SUF(c, -ilogbw);                                                  \
      d = scalbn_##SUF(d, -ilogbw);                                                  \
    } else if (m == 0) {                                                             \
      logbw_inf = 1; /* logb(0) is -infinity */                                      \
    }                                                                                \
    T denom = c * c + d * d;                                                         \
    cplx_##SUF r;                                                                    \
    r.p[0] = scalbn_##SUF((a * c + b * d) / denom, -ilogbw);                         \
    r.p[1] = scalbn_##SUF((b * c - a * d) / denom, -ilogbw);                         \
    if (isnan_##SUF(r.p[0]) && isnan_##SUF(r.p[1])) {                                \
      if (denom == 0 && (!isnan_##SUF(a) || !isnan_##SUF(b))) {                      \
        r.p[0] = copysign_##SUF(inf, c) * a;                                         \
        r.p[1] = copysign_##SUF(inf, c) * b;                                         \
      } else if ((isinf_##SUF(a) || isinf_##SUF(b)) && isfinite_##SUF(c) &&          \
                 isfinite_##SUF(d)) {                                                \
        a = copysign_##SUF(isinf_##SUF(a) ? 1 : 0, a);                               \
        b = copysign_##SUF(isinf_##SUF(b) ? 1 : 0, b);                               \
        r.p[0] = inf * (a * c + b * d);                                              \
        r.p[1] = inf * (b * c - a * d);                                              \
      } else if (logbw_inf && m != 0 && isfinite_##SUF(a) && isfinite_##SUF(b)) {    \
        c = copysign_##SUF(isinf_##SUF(c) ? 1 : 0, c);                               \
        d = copysign_##SUF(isinf_##SUF(d) ? 1 : 0, d);                               \
        r.p[0] = 0 * (a * c + b * d);                                                \
        r.p[1] = 0 * (b * c - a * d);                                                \
      }                                                                              \
    }                                                                                \
    return r.z;                                                                      \
  }

DEFINE(d, double, 7)
DEFINE(x, long double, 9)

/* float: computed in double, whose range and precision make the
   intermediate results exact or nearly so, then rounded once */
typedef union {
  float _Complex z;
  float p[2];
} cplx_s;

float _Complex __mulsc3(float a, float b, float c, float d) {
  cplx_d w;
  cplx_s r;
  w.z = __muldc3(a, b, c, d);
  r.p[0] = (float)w.p[0];
  r.p[1] = (float)w.p[1];
  return r.z;
}

float _Complex __divsc3(float a, float b, float c, float d) {
  cplx_d w;
  cplx_s r;
  w.z = __divdc3(a, b, c, d);
  r.p[0] = (float)w.p[0];
  r.p[1] = (float)w.p[1];
  return r.z;
}
