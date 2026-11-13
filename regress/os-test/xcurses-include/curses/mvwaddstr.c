/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddstr
#undef mvwaddstr
#endif
int (*foo)(WINDOW *, int, int, const char *) = mvwaddstr;
int main(void) { return 0; }
