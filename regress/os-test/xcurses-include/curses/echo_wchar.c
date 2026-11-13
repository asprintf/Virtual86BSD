/*[SUSv2]*/
#include <curses.h>
#ifdef echo_wchar
#undef echo_wchar
#endif
int (*foo)(const cchar_t *) = echo_wchar;
int main(void) { return 0; }
