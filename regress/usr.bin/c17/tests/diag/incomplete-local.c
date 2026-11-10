/* expect error: incomplete type */
struct S;
void f(void) { struct S s; }
