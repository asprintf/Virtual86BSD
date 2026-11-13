/*[SUSv2]*/
#include <curses.h>
#ifdef mvins_wch
#undef mvins_wch
#endif
int (*foo)(int, int, const cchar_t *) = mvins_wch;
int main(void) { return 0; }
