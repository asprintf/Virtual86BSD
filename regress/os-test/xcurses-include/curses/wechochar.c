/*[SUSv2]*/
#include <curses.h>
#ifdef wechochar
#undef wechochar
#endif
int (*foo)(WINDOW *, const chtype) = wechochar;
int main(void) { return 0; }
