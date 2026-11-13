/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinstr
#undef mvwinstr
#endif
int (*foo)(WINDOW *, int, int, char *) = mvwinstr;
int main(void) { return 0; }
