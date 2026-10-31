/* expect error: switch jumps into scope */
void f(int n, int x) { switch (x) { int a[n]; case 1: a[0] = 1; } }
