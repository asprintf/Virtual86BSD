/* expect error: read-only */
void f(void) { const int x = 0; x++; }
