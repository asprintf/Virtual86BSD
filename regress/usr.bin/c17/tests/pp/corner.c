/* Corner cases of macro replacement (expected output from gcc -std=c17 -E) */
#define f(x) x
f(f)(1);
#define foo foo
foo;
#define a a b
#define b a
a; b;
#undef a
#undef b
#define EMPTY
EMPTY # define notadirective
#define g(x, y) x y
g(1
  ,
  2);
#define str(x) #x
#define xstr(x) str(x)
str(  leading   and   trailing  );
str("a\\b" '\'' "\n");
str(@ \ $);
xstr(__LINE__);
#if -1 > 0u
unsigned_compare_ok;
#endif
#if 'A' == 65 && '\377' < 0
char_signed_ok;
#endif
#if (2 || 1/0) && !(0 && 1/0)
short_circuit_ok;
#endif
#if 0x7fffffffffffffff + 0 > 0 && 18446744073709551615u == -1
intmax_ok;
#endif
%:define DIG <: :> <% %> %:
DIG;
??=define TRI 3 ??/
+ 4
TRI ??( ??) ??< ??> ??! ??- ??';
#define obj (obj + 1)
#define fn(x) fn(x + obj)
fn(fn(0));
#define LPAREN (
#define F(x) [x]
#define G F LPAREN 1 )
G;
#define cat(a, b) a ## b
cat(1, e+) 2;
cat(., 5);
cat(<, <=);
cat(L, "wide");
cat(u8, "utf8");
#define VA(fmt, ...) printf(fmt, __VA_ARGS__)
VA("%d %d", 1, (2, 3));
#define VA2(...) [__VA_ARGS__]
VA2();
VA2(,);
VA2( a , b );
