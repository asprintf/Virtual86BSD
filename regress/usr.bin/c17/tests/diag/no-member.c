/* expect error: has no member named 'b' */
struct S { int a; };
void f(struct S s) { (void)s.b; }
