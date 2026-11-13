/*[SUSv2]*/
#include <curses.h>
#ifdef mvget_wch
#undef mvget_wch
#endif
int (*foo)(int, int, wint_t *) = mvget_wch;
int main(void) { return 0; }
