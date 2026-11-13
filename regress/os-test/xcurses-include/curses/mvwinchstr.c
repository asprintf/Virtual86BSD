/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinchstr
#undef mvwinchstr
#endif
int (*foo)(WINDOW *, int, int, chtype *) = mvwinchstr;
int main(void) { return 0; }
