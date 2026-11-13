/*[SUSv2]*/
#include <curses.h>
#ifdef werase
#undef werase
#endif
int (*foo)(WINDOW *) = werase;
int main(void) { return 0; }
