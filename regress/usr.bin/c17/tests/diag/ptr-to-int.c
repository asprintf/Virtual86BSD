/* expect warning: makes integer from pointer */
void f(int *p) { int x = p; (void)x; }
