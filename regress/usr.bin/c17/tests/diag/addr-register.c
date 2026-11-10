/* expect error: register */
void f(void) { register int x = 0; int *p = &x; (void)p; }
