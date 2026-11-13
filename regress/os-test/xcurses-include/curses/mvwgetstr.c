/*[SUSv2]*/
#include <curses.h>
#ifdef mvwgetstr
#undef mvwgetstr
#endif
int (*foo)(WINDOW *, int, int, char *) = mvwgetstr;
int main(void) { return 0; }
