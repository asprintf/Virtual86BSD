/*[SUSv2]*/
#include <curses.h>
#ifdef mvwget_wch
#undef mvwget_wch
#endif
int (*foo)(WINDOW *, int, int, wint_t *) = mvwget_wch;
int main(void) { return 0; }
