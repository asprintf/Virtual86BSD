/* expect error: not an integer constant expression */
void f(int x, int y) { switch (x) { case y: ; } }
