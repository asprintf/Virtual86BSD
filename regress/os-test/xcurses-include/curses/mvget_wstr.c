/*[SUSv2]*/
#include <curses.h>
#ifdef mvget_wstr
#undef mvget_wstr
#endif
int (*foo)(int, int, wint_t *) = mvget_wstr;
int main(void) { return 0; }
