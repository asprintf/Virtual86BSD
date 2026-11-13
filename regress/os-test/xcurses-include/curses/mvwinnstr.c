/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinnstr
#undef mvwinnstr
#endif
int (*foo)(WINDOW *, int, int, char *, int) = mvwinnstr;
int main(void) { return 0; }
