/* expect error: type mismatch in conditional */
struct S { int a; };
void f(int c, struct S s) { (void)(c ? s : 1); }
