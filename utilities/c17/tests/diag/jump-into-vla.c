/* expect error: jump into scope of identifier with variably modified type */
void f(int n) { goto l; { int a[n]; l: a[0] = 1; } }
