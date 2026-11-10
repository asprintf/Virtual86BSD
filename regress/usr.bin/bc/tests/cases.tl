/*
 * cases.tl - the cases of difftest.tl (and the seeds of fuzz.tl): a bc
 * program (and options and files) whose standard output must be the same
 * as GNU bc's.  Fields: name, prog (the standard input), and optionally
 * args, env (variables set), files (name: contents, given as operands),
 * differs (why GNU bc differs).
 */

var CASES = [
    {name: "spec-pi",
     prog: "scale = 10\n\"pi equals \"\n104348 / 33215\n"},
    {name: "spec-e",
     prog: "scale = 20\n"
           "define e(x){\n"
           "    auto a, b, c, i, s\n"
           "    a = 1\n"
           "    b = 1\n"
           "    s = 1\n"
           "    for (i = 1; 1 == 1; i++){\n"
           "        a = a*x\n"
           "        b = b*i\n"
           "        c = a/b\n"
           "        if (c == 0) {\n"
           "             return(s)\n"
           "        }\n"
           "        s = s+c\n"
           "    }\n"
           "}\n"
           "for (i = 1; i <= 10; ++i) {\n"
           "    e(i)\n"
           "}\n"},
    {name: "arith-scales",
     prog: "1+2*3-4/5\n"
           "scale=3; 1/3; 2/3; -2/3; 10/4; 7%3; -7%3; 7%-3; 7.5%2; 0.5*0.25; 1.5*1.5\n"
           "scale=0; 7/2; -7/2; 7%2; 2.5*2.5; 3.2/1\n"
           "scale=10; 1.25^3; 2^-3; (-2)^3; (-2)^-3; 0^0; 10^20; .5^10; 1.000^2\n"
           "scale=2; 3.14159 + 0; 3.14159 * 1; 3.14159 / 1; -3.14159 - 0\n"},
    {name: "mod-scale",
     prog: "scale=5; 7%3; 22%7; 1%0.3; -1%0.3\n"
           "scale=0; 22%7; -22%7; 22%-7; 2.5%1\n"
           "scale=2; 2.5%1; 10.12%3.4\n"},
    {name: "power",
     prog: "2^100; 3^-1; scale=5; 3^-1; 1.1^10; 1.01^100; (-1.5)^3; 2^0; 0^5\n"
           "scale=0; 1.5^2; 1.5^3; 12.345^2\n"},
    {name: "sqrt",
     prog: "sqrt(16); sqrt(2); sqrt(0.25); scale=10; sqrt(2); sqrt(1000000); sqrt(.0001); sqrt(123456789.123)\n"
           "scale(sqrt(2)); scale=0; scale(sqrt(2.12345)); sqrt(2.12345)\n"},
    {name: "length-scale",
     prog: "length(123); length(123.456); length(0.001); length(1.000); length(100)\n"
           "scale(123); scale(1.000); scale(1/3); scale=4; scale(1/3); scale(2.5*2.5)\n"},
    {name: "output-bases",
     prog: "obase=2; 10; 255; -5; 0.5; 0.1\n"
           "obase=8; 64; 8.5\n"
           "obase=16; 255; 4095.75; -1; 0.1; 255.255\n"
           "obase=3; 1/3; scale=5; 1/3\n"
           "obase=17; 300\n"
           "obase=25; 1024; 1024.5; .04\n"
           "obase=125; 1024\n"
           "obase=1000; 123456789\n"
           "obase=10; 1/3\n"},
    {name: "input-bases",
     prog: "ibase=16; FF; A.8; .C; 1F.F; 10\n"
           "ibase=A; ibase=2; 1010; 1.1; 111.111\n"
           "ibase=A; ibase=8; 777; 7.7; 10\n"
           "ibase=A\n"
           "A; F; 15\n"},
    {name: "single-digit-ibase",
     prog: "ibase=16\nobase=A\nFF\nibase=A\nFF\n"},
    {name: "long-numbers",
     prog: "2^500\n"
           "scale=100; 1/7; -1/7; sqrt(2)\n"
           "obase=16; 2^300\n"
           "obase=17; 2^200\n"},
    {name: "string-column",
     prog: "\"pi equals \"\n2^300\n\"x\"\n\"\n\"\n2^300\n"},
    {name: "line-length",
     prog: "2^300\n",
     env: {"BC_LINE_LENGTH": "30"}},
    {name: "line-length-0",
     prog: "2^300\n",
     env: {"BC_LINE_LENGTH": "0"}},
    {name: "variables-arrays",
     prog: "a = 5; b[3] = 7; a; b[3]; b[2]; a[3] = 2; a; a[3]\n"
           "c = 1.50; c; c++; c; ++c; c--; --c; c\n"
           "d[1.9] = 4; d[1]; scale = 3; e = 1/3; e; e *= 3; e; e += 1; e -= 0.5; e; e /= 2; e; e %= 1; e; e ^= 2; e\n"},
    {name: "registers",
     prog: "scale; ibase; obase\n"
           "scale = 2.9; scale; ibase = 16; ibase; ibase = A; obase = 16; obase; obase = A\n"
           "scale(scale); x = scale++; x; scale; ++scale\n"},
    {name: "print-assign",
     prog: "a = 5\n(a = 6)\n(a += 1)\na\na++\n++a\n-a\n"},
    {name: "functions",
     prog: "define f(x) {\n"
           "    return (x * 2)\n"
           "}\n"
           "define g(x, y) { return (x + y) }\n"
           "define h() {\n"
           "    return\n"
           "}\n"
           "define k(x) {\n"
           "}\n"
           "f(3); g(2, 3); h(); k(5)\n"
           "define f(x) { return (x * 3) }\n"
           "f(3)\n"
           "define r(n) {\n"
           "    if (n <= 1) return (1)\n"
           "    return (n * r(n - 1))\n"
           "}\n"
           "r(25)\n"
           "define fib(n) {\n"
           "    if (n < 2) return (n)\n"
           "    return (fib(n - 1) + fib(n - 2))\n"
           "}\n"
           "fib(15)\n"},
    {name: "auto-dynamic-scope",
     prog: "define f() {\n"
           "    auto a\n"
           "    a = 10\n"
           "    return (g())\n"
           "}\n"
           "define g() {\n"
           "    return (a)\n"
           "}\n"
           "a = 1\n"
           "f(); g(); a\n"
           "define h(a) {\n"
           "    a = 99\n"
           "    return (g())\n"
           "}\n"
           "h(5); a\n"},
    {name: "array-params",
     prog: "a[0] = 1; a[1] = 2; a[2] = 3\n"
           "define s(x[], n) {\n"
           "    auto i, t\n"
           "    for (i = 0; i < n; i++) t += x[i]\n"
           "    x[0] = 100\n"
           "    return (t)\n"
           "}\n"
           "s(a[], 3); a[0]\n"
           "define c(n) {\n"
           "    auto b[]\n"
           "    b[n] = n\n"
           "    return (b[n] + b[n - 1])\n"
           "}\n"
           "c(5); b[5]\n"
           "define t(n, x[]) { return (x[n]) }\n"
           "t(2, a[])\n"},
    {name: "constants-in-functions",
     prog: "define f() { return (10) }\n"
           "f()\n"
           "ibase = 16\n"
           "f()\n"
           "ibase = A\n"
           "f()\n"},
    {name: "control",
     prog: "for (i = 0; i < 5; i++) i\n"
           "i = 0\n"
           "while (i < 3) { i; i = i + 1 }\n"
           "for (i = 0; i < 10; i++) { if (i == 3) break; i }\n"
           "if (1 == 1) \"yes\n"
           "\"\n"
           "if (1 != 1) \"no\n"
           "\"\n"
           "i = 0; while (1) { if (++i > 4) break; }; i\n"
           "for (i = 10; i > 0; i = i - 3) i\n"},
    {name: "relational",
     prog: "if (2 < 3) 1\n"
           "if (2 > 3) 2\n"
           "if (2 <= 2) 3\n"
           "if (2 >= 3) 4\n"
           "if (2 == 2.0) 5\n"
           "if (2 != 2) 6\n"
           "if (-1 < 0) 7\n"
           "if (0.001 > 0) 8\n"},
    {name: "strings",
     prog: "\"hello, world\n\"\n\"a\\nb\"\n\"\n\"\n\"multi\nline\n\"\n\"tab\there\n\"\n"},
    {name: "comments-continuations",
     prog: "/* a comment */ 1 + /* inline */ 2\n"
           "/* a\n"
           "   multi-line comment */\n"
           "3 + \\\n"
           "4\n"
           "12\\\n"
           "34\n"
           "1.5\\\n"
           "5\n"},
    {name: "semicolons",
     prog: "1; 2;; 3;\n;\n\n4\n"},
    {name: "quit",
     prog: "1\n2\nquit\n3\n"},
    {name: "quit-in-definition",
     prog: "1\ndefine f() {\n  quit\n}\n2\n"},
    {name: "quit-mid-line",
     prog: "1; 2; quit\n"},
    {name: "no-final-newline",
     prog: "1+1",
     differs: "a last line without a newline is executed; GNU bc reports a syntax error"},
    {name: "divide-by-zero",
     prog: "1/0\n2\nx = 3; y = x / 0; 4\n5\n0^-1\n6\n"},
    {name: "errors-recover",
     prog: "1 +\n2\n)\n3\nsqrt(-4)\n4\n"},
    {name: "math-lib",
     args: ["-l"],
     prog: "scale\n"
           "e(0); s(0); c(0); a(0); l(1); j(0,0)\n"
           "e(1); e(-1); e(10); e(-10); l(2); l(0.5); l(100); s(1); c(1); a(1); a(-1); a(100)\n"
           "s(10); c(-3); j(2,3.5); j(-1,2); j(3,-2); j(1, 10)\n"
           "scale=5; e(1.5); s(1); scale\n"
           "scale=0; e(1); a(1)\n"
           "scale=50; s(100); e(1); l(10); a(.2); c(1)\n"},
    {name: "math-pi",
     args: ["-l"],
     prog: "scale=500; 4*a(1)\n"},
    {name: "math-redefine",
     args: ["-l"],
     prog: "define s(x) { return (x) }\ns(5)\n"},
    {name: "ext-else-print",
     prog: "if (1 > 2) 1 else 2\n"
           "print \"a=\", 5, \"\\n\"\n"
           "print 1/3, \"\\n\"\n"
           "x = 0; while (x < 5) { x++; if (x == 2) continue; x }\n"
           "1 && 0; 1 || 0; !0; !5; 3 > 2 && 2 > 1\n"},
    {name: "ext-long-names",
     prog: "alpha = 3; beta_2 = 4; alpha * beta_2\n"
           "define sq(value) { return (value * value) }\n"
           "sq(alpha)\n"},
    {name: "files",
     files: {"one.bc": "a = 5\ndefine f(x) { return (x + a) }\n", "two.bc": "f(1)\n"},
     prog: "f(2)\n"},
    {name: "big-mult",
     prog: "x = 2^2000 - 1\n"
           "y = 3^1000 + 7\n"
           "x * y % 1000000007\n"
           "x / y\n"
           "scale = 50; x / y\n"},
    {name: "big-division",
     prog: "scale=0; (10^200 + 12345) / (10^100 + 1); (10^200 + 12345) % (10^100 + 1)\n"
           "scale=20; 123456789012345678901234567890 / 987654321\n"},
    {name: "digits-beyond-ibase",
     prog: "ibase=2\n3\n3.1\n31\n13\n.3\n3.3\n10.3\nibase=8\n9\n99\n9.9\n"},
    {name: "math-exact-points",
     args: ["-l"],
     prog: "scale=40; c(0); s(0); e(0); l(1); a(0); x = c(0) - 1; x\n"
           "scale=5; c(0); c(10)\n"},
    {name: "return-forms",
     prog: "define f(n) { if (n < 2) return 1; return n * f(n - 1) }\n"
           "f(20)\n"
           "define h(x) { return (x) + 1 }\n"
           "h(2)\n"},
    {name: "return-empty-parens",
     prog: "define g() { return () }\ng()\n",
     differs: "POSIX allows return (); GNU bc reports a syntax error"},
    {name: "ibase-after-error",
     prog: "ibase = 2; 1/0; ibase = A\nibase\n"}
];
