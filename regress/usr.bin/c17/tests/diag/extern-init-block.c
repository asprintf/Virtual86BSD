/* expect error: has both 'extern' and initializer */
void f(void) { extern int x = 1; }
