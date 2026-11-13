/*[SUSv2]*/
#include <curses.h>
#ifdef ungetch
#undef ungetch
#endif
int (*foo)(int) = ungetch;
int main(void) { return 0; }
