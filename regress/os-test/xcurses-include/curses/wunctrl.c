/*[SUSv2]*/
#include <curses.h>
#ifdef wunctrl
#undef wunctrl
#endif
wchar_t *(*foo)(cchar_t *) = wunctrl;
int main(void) { return 0; }
