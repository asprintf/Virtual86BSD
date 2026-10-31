/* expect error: duplicate case value */
void f(int x) { switch (x) { case 1: case 1: ; } }
