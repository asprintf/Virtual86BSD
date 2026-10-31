/* expect error: a label can only be part of a statement */
void f(void) { l: int x; (void)x; }
