#!/usr/bin/env python3
# Copyright (c) 2026, asprintf
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# ref.py - checks math/__ddl.c (built as ./host by run.sh) against exact values.
#   python3 -I ref.py [-x] [-y] FUNC N LO HI [SEED]    N random points, |a| in
#       [2^LO, 2^(HI+1)), random 64-bit mantissas; -x: real part 0, -y: imaginary part 0
#   python3 -I ref.py special                           tgamma at the factorials and edges
# FUNC: cexp csinh ccosh ctanh catanh casinh cacosh cpow tgamma sinh cosh asinh.
# Every handled point must equal the correctly rounded long double of an
# 80-digit reference (Decimal), the sign of a zero result included for the
# -x runs.  Exit status 1 if one is wrong.
import math, os, random, subprocess, sys
from decimal import Decimal as D, getcontext
from fractions import Fraction as F

HOST = os.environ.get('DDLHOST') or os.path.join(os.path.dirname(os.path.abspath(__file__)), 'host')
getcontext().prec = 90

def atan_(x):
    k = 0
    while abs(x) > D('0.1'):
        x = x / (1 + (1 + x * x).sqrt()); k += 1
    x2 = x * x; t = x; s = x; n = 1
    while abs(t) > D(10) ** -95:
        t = -t * x2; n += 2; s += t / n
    return s * 2 ** k

PI = 16 * atan_(D(1) / 5) - 4 * atan_(D(1) / 239)

def atan2_(y, x):
    if x > 0: return atan_(y / x)
    if x < 0: return atan_(y / x) + (PI if not y.is_signed() else -PI)
    return PI / 2 if y > 0 else -PI / 2

def sincos_(x):
    x = x % (2 * PI)
    if x > PI: x -= 2 * PI
    h = x / 64; x2 = h * h
    s = h; c = D(1); ts = h; tc = D(1); n = 1
    while abs(ts) > D(10) ** -100 and n < 200:
        tc = -tc * x2 / (n * (n + 1)); ts = -ts * x2 / ((n + 1) * (n + 2)); c += tc; s += ts; n += 2
    for _ in range(6):
        s, c = 2 * s * c, c * c - s * s
    return s, c

def sinhcosh(x):
    e = x.exp(); return (e - 1 / e) / 2, (e + 1 / e) / 2

# complex numbers as (re, im)
def cm(a, b): return (a[0] * b[0] - a[1] * b[1], a[0] * b[1] + a[1] * b[0])
def cd(a, b):
    d = b[0] * b[0] + b[1] * b[1]
    return ((a[0] * b[0] + a[1] * b[1]) / d, (a[1] * b[0] - a[0] * b[1]) / d)
def ca(a, b): return (a[0] + b[0], a[1] + b[1])
def cs(a, b): return (a[0] - b[0], a[1] - b[1])
def cabs(a): return (a[0] * a[0] + a[1] * a[1]).sqrt()
def clog(a): return (cabs(a).ln(), atan2_(a[1], a[0]))
def cexp(a):
    s, c = sincos_(a[1]); e = a[0].exp(); return (e * c, e * s)
def csqrt(a):
    m = cabs(a)
    if a[0] >= 0:
        r = ((m + a[0]) / 2).sqrt(); return (r, a[1] / (2 * r))
    t = ((m - a[0]) / 2).sqrt(); return (abs(a[1]) / (2 * t), t if a[1] >= 0 else -t)
ONE = (D(1), D(0))

# ln Gamma by Stirling's series from z >= 300 (Bernoulli numbers by recurrence)
def bernoulli(n):
    A = [F(0)] * (n + 1)
    for m in range(n + 1):
        A[m] = F(1, m + 1)
        for j in range(m, 0, -1): A[j - 1] = j * (A[j - 1] - A[j])
    return A[0]
B2 = {}
def lngamma(z):
    s = (z - D('0.5')) * z.ln() - z + (2 * PI).ln() / 2
    for k in range(1, 50):
        if k not in B2: B2[k] = bernoulli(2 * k)
        b = B2[k]; s += D(b.numerator) / D(b.denominator) / (2 * k * (2 * k - 1)) / z ** (2 * k - 1)
    return s
def gamma(x):
    p = D(1); z = x
    while z < 300: p *= z; z += 1
    return lngamma(z).exp() / p

def f_csinh(z, w):
    sh, ch = sinhcosh(z[0]); s, c = sincos_(z[1]); return (sh * c, ch * s)
def f_ccosh(z, w):
    sh, ch = sinhcosh(z[0]); s, c = sincos_(z[1]); return (ch * c, sh * s)
def f_ctanh(z, w):                      # sinh/cosh cancels about e^(4|x|): more digits
    old = getcontext().prec
    getcontext().prec = 120 + int(4 * abs(float(z[0])))
    try: r = cd(f_csinh(z, w), f_ccosh(z, w))
    finally: getcontext().prec = old
    return (+r[0], +r[1])
def f_catanh(z, w):
    l = cs(clog(ca(ONE, z)), clog(cs(ONE, z))); return (l[0] / 2, l[1] / 2)
def f_casinh(z, w): return clog(ca(z, csqrt(ca(cm(z, z), ONE))))
def f_cacosh(z, w): return clog(ca(z, cm(csqrt(cs(z, ONE)), csqrt(ca(z, ONE)))))
def f_tgamma(z, w):
    x = z[0]
    if x > 0: return (gamma(x), D(0))
    s, c = sincos_(PI * x)
    return (PI / (s * gamma(1 - x)), D(0))
def f_asinh(z, w):
    x = z[0]; r = (abs(x) + (x * x + 1).sqrt()).ln(); return (r if x >= 0 else -r, D(0))
FUN = {'cexp': lambda z, w: cexp(z), 'csinh': f_csinh, 'ccosh': f_ccosh, 'ctanh': f_ctanh,
       'catanh': f_catanh, 'casinh': f_casinh, 'cacosh': f_cacosh, 'cpow': lambda z, w: cexp(cm(w, clog(z))),
       'tgamma': f_tgamma, 'sinh': lambda z, w: (sinhcosh(z[0])[0], D(0)),
       'cosh': lambda z, w: (sinhcosh(z[0])[1], D(0)), 'asinh': f_asinh}

def hexf(fr):                           # Fraction -> hex float (a 64-bit mantissa)
    if fr == 0: return '0x0p+0'
    sgn = '-' if fr < 0 else ''; fr = abs(fr)
    e = fr.numerator.bit_length() - fr.denominator.bit_length()
    if F(2) ** e > fr: e -= 1
    frac = (fr / F(2) ** e - 1) * 2 ** 64
    return '%s0x1.%016xp%+d' % (sgn, int(frac), e)

def parse(s):                           # hex float -> Fraction
    sgn = -1 if s.startswith('-') else 1; s = s.lstrip('-')
    m, e = s[2:].split('p'); ip, fp = (m.split('.') + [''])[:2]
    v = F(int(ip or '0', 16)) + (F(int(fp, 16), 16 ** len(fp)) if fp else 0)
    return sgn * v * F(2) ** int(e)

def round_ld(x):                        # Decimal -> nearest long double, as a Fraction
    fr = F(x)
    if fr == 0: return fr
    sgn = 1 if fr > 0 else -1; fr = abs(fr)
    e = fr.numerator.bit_length() - fr.denominator.bit_length()
    if F(2) ** e > fr: e -= 1
    q = fr / F(2) ** (e - 63)
    n = q.numerator // q.denominator; r = q - n
    if r > F(1, 2) or (r == F(1, 2) and n % 2 == 1): n += 1
    return sgn * F(n) * F(2) ** (e - 63)

def to_dec(v): return D(v.numerator) / D(v.denominator)

def run_host(fn, pts):
    inp = ''.join('%s %s %s %s %s\n' % ((fn,) + tuple(pts_hex(p))) for p in pts)
    out = subprocess.run([HOST], input=inp, capture_output=True, text=True).stdout.split('\n')
    return [l.split() for l in out if len(l.split()) >= 8]

def pts_hex(p):
    return [('-0x0p+0' if random.random() < .5 else '0x0p+0') if v == 0 else hexf(v) for v in p]

def check(fn, rows, zx):
    handled = bad = 0; worst = []
    for t in rows:
        if t[5] != '1': continue
        a, b, c, d = [parse(x) for x in t[1:5]]
        z = (to_dec(a), D('-0') if b == 0 and t[2].startswith('-') else to_dec(b))
        er, ei = FUN[fn](z, (to_dec(c), to_dec(d)))
        rr, ri = round_ld(er), round_ld(ei)
        handled += 1
        if parse(t[6]) != rr or parse(t[7]) != ri:
            bad += 1
            if len(worst) < 3: worst.append((t[1:5], t[6], hexf(rr), t[7], hexf(ri)))
        if zx and fn in ('csinh', 'ccosh', 'ctanh', 'catanh'):      # sign of the zero part
            sx = t[1].startswith('-')
            if fn == 'csinh': want, got = sx != (math.cos(float(z[1])) < 0), t[6]
            elif fn == 'ccosh': want, got = sx != (math.sin(float(z[1])) < 0), t[7]
            else: want, got = sx, t[6]
            if parse(got) != 0 or got.startswith('-') != want:
                bad += 1
                if len(worst) < 3: worst.append(('zero sign', t[1:5], got, want))
    return handled, bad, worst

def special():
    pts = [F(n) for n in range(1, 26)] + [F(1, 2), F(-1, 2), F(-5, 2), F(3, 2), F(-1234, 100), F(9001, 100), F(1500),
           F(-1499) + F(1, 2), F(1, 2 ** 64), F(-1, 2 ** 64), F(-3) + F(1, 2 ** 60), F(-3) - F(1, 2 ** 60),
           F(-7) + F(1, 2 ** 30), F(1) + F(1, 2 ** 63), F(2) - F(1, 2 ** 63), F(170) + F(1, 3)]
    pts = [round_ld(to_dec(p)) for p in pts]
    rows = run_host('tgamma', [[p, F(0), F(0), F(0)] for p in pts])
    bad = 0
    for p, t in zip(pts, rows):
        if t[5] != '1': print('  not handled', float(p)); bad += 1; continue
        want = round_ld(f_tgamma((to_dec(p), D(0)), None)[0])
        if p.denominator == 1 and 1 <= p <= 25: want = F(math.factorial(int(p) - 1))
        if parse(t[6]) != want: bad += 1; print('  WRONG', float(p), t[6], hexf(want))
    print('tgamma special: %d points, %d wrong' % (len(pts), bad))
    return bad

def main():
    a = sys.argv[1:]
    if a[:1] == ['special']: sys.exit(1 if special() else 0)
    zx = zy = False
    while a and a[0] in ('-x', '-y'):
        if a.pop(0) == '-x': zx = True
        else: zy = True
    fn, n, lo, hi = a[0], int(a[1]), int(a[2]), int(a[3])
    random.seed(int(a[4]) if len(a) > 4 else 1)
    pts = []
    for _ in range(n):
        p = [(1 if random.random() < .5 else -1) * F(random.getrandbits(64) | (1 << 63), 2 ** 63) * F(2) ** random.randint(lo, hi) for _ in range(4)]
        if zx: p[0] = F(0)
        if zy: p[1] = F(0)
        pts.append(p)
    handled, bad, worst = check(fn, run_host(fn, pts), zx)
    print('%-7s%s points %d, handled %d, wrong %d' % (fn, ' (zero real)' if zx else ' (zero imag)' if zy else '', n, handled, bad))
    for w in worst: print('   ', w)
    sys.exit(1 if bad else 0)

main()
