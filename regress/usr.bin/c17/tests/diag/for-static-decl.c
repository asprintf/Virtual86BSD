/* expect error: non-variable */
void f(void) { for (static int i = 0; i < 1; ) ; }
