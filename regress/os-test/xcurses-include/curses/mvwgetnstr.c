/*[SUSv2]*/
#include <curses.h>
#ifdef mvwgetnstr
#undef mvwgetnstr
#endif
int (*foo)(WINDOW *, int, int, char *, int) = mvwgetnstr;
int main(void) { return 0; }
