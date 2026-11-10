/* expect error: invalid type argument of '->' */
struct S { int a; };
void f(struct S s) { (void)s->a; }
