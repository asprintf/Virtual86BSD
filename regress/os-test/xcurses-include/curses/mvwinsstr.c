/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinsstr
#undef mvwinsstr
#endif
int (*foo)(WINDOW *, int, int, const char *) = mvwinsstr;
int main(void) { return 0; }
