/*[SUSv2]*/
#include <curses.h>
#ifdef mvwget_wstr
#undef mvwget_wstr
#endif
int (*foo)(WINDOW *, int, int, wint_t *) = mvwget_wstr;
int main(void) { return 0; }
