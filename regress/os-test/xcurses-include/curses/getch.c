/*[SUSv2]*/
#include <curses.h>
#ifdef getch
#undef getch
#endif
int (*foo)(void) = getch;
int main(void) { return 0; }
