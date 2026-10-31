/* expect error: read-only */
struct S { const int a; };
void f(struct S *s, struct S t) { *s = t; }
