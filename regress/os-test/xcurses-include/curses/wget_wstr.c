/*[SUSv2]*/
#include <curses.h>
#ifdef wget_wstr
#undef wget_wstr
#endif
int (*foo)(WINDOW *, wint_t *) = wget_wstr;
int main(void) { return 0; }
