/* expect error: lvalue required */
void g(void);
void f(void) { g = 0; }
