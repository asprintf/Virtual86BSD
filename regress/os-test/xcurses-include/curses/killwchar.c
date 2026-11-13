/*[SUSv2]*/
#include <curses.h>
#ifdef killwchar
#undef killwchar
#endif
int (*foo)(wchar_t *) = killwchar;
int main(void) { return 0; }
