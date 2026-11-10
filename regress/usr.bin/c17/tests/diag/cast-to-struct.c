/* expect error: non-scalar type */
struct S { int a; };
void f(int x) { (void)(struct S)x; }
