/* expect warning: discards qualifiers */
void f(const int *p) { int *q = p; (void)q; }
