/*[SUSv2]*/
#include <curses.h>
#ifdef wins_wch
#undef wins_wch
#endif
int (*foo)(WINDOW *, const cchar_t *) = wins_wch;
int main(void) { return 0; }
