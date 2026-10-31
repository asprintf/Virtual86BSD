/* expect error: wrong type argument to increment */
struct S { int a; };
void f(struct S s) { s++; }
