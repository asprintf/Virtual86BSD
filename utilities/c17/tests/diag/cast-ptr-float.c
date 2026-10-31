/* expect error: between pointer and floating */
void f(int *p) { (void)(double)p; }
