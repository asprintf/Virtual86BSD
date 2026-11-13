/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinsnstr
#undef mvwinsnstr
#endif
int (*foo)(WINDOW *, int, int, const char *, int) = mvwinsnstr;
int main(void) { return 0; }
