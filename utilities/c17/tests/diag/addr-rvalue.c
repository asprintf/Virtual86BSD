/* expect error: lvalue required as unary '&' */
void f(int a) { int *p = &(a + 1); (void)p; }
