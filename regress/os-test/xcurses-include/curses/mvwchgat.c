/*[SUSv2]*/
#include <curses.h>
#ifdef mvwchgat
#undef mvwchgat
#endif
int (*foo)(WINDOW *, int, int, int, attr_t, short, const void *) = mvwchgat;
int main(void) { return 0; }
