/* expect error: void value not ignored */
void g(void);
void f(void) { int x = g(); (void)x; }
