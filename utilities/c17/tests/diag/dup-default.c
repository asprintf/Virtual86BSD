/* expect error: multiple default labels */
void f(int x) { switch (x) { default: default: ; } }
