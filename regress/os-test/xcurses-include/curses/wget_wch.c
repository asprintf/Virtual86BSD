/*[SUSv2]*/
#include <curses.h>
#ifdef wget_wch
#undef wget_wch
#endif
int (*foo)(WINDOW *, wint_t *) = wget_wch;
int main(void) { return 0; }
