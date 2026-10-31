/* expect error: bit-field */
struct S { int a : 3; } s;
unsigned long x = sizeof(s.a);
