/* expect error: bit-field */
struct S { int a : 3; };
void f(struct S *s) { int *p = &s->a; (void)p; }
