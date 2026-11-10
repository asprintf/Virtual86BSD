/* expect error: incomplete type */
struct S;
unsigned long x = sizeof(struct S);
