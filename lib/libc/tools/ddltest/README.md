# ddltest

Tests the double-double functions of `lib/libc/math/__ddl.c` on the host,
against exact values.  The library is built for the FreeBSD ABI and does not
run on the host, so `__ddl.c` is compiled natively (x87 long double is the same
format) with `host.c`, a driver that reads `FUNC a b c d` lines of hex floats.

* `run.sh [OBJDIR]` builds it and runs the matrix below (`POINTS=n`, 200 by
  default, random points per run); status 1 if a result is wrong
* `ref.py` makes the points, runs `host` and compares every handled result with
  the correctly rounded long double of an 80-digit reference (`decimal`):
  `python3 -I ref.py [-x] [-y] FUNC N LO HI [SEED]`, or `python3 -I ref.py special`

Functions: `cexp csinh ccosh ctanh catanh casinh cacosh cpow` (complex, `a b` is
the argument, `c d` the exponent of `cpow`), `tgamma sinh cosh asinh`.  The
reference uses definitions other than the code's (`tanh` as `sinh / cosh`,
`atanh` as `(ln(1+z) - ln(1-z)) / 2`, ln Gamma by Stirling from 300 with
Bernoulli numbers made by recurrence), so a wrong formula or constant shows.

The matrix: every function at three ranges of magnitude (2^-14 to 2^-2,
2^-2 to 2^3, 2^0 to 2^10); the complex ones with a zero real part (and the
sign of the zero parts of `csinh`, `ccosh`, `ctanh` and `catanh` checked),
`catanh` and `cpow` with a zero imaginary part; and `tgamma` at the factorials,
half integers, the edges of its range and arguments next to the poles.  Points
outside the range a function handles are reported as not handled and not
compared (the library falls back to its ordinary code there).
