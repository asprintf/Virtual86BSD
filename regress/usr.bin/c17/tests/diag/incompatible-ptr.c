/* expect warning: incompatible pointer types */
void f(void) { int *p; long *q = 0; p = q; (void)p; }
