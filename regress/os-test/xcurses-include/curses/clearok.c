/*[SUSv2]*/
#include <curses.h>
#ifdef clearok
#undef clearok
#endif
int (*foo)(WINDOW *, bool) = clearok;
int main(void) { return 0; }
