/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddstr
#undef mvaddstr
#endif
int (*foo)(int, int, const char *) = mvaddstr;
int main(void) { return 0; }
