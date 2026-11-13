/*[SUSv2]*/
#include <curses.h>
#ifdef mvinnstr
#undef mvinnstr
#endif
int (*foo)(int, int, char *, int) = mvinnstr;
int main(void) { return 0; }
