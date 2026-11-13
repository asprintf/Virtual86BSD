/*[SUSv2]*/
#include <curses.h>
#ifdef mvgetnstr
#undef mvgetnstr
#endif
int (*foo)(int, int, char *, int) = mvgetnstr;
int main(void) { return 0; }
