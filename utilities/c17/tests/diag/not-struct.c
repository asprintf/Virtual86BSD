/* expect error: scalar is required */
struct S { int a; };
void f(struct S s) { (void)!s; }
