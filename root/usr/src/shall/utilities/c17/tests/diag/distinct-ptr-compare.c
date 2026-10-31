/* expect warning: distinct pointer types */
int f(int *p, long *q) { return p == q; }
