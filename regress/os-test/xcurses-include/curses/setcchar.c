/*[SUSv2]*/
#include <curses.h>
#ifdef setcchar
#undef setcchar
#endif
int (*foo)(cchar_t *, const wchar_t *, const attr_t, short, const void *) = setcchar;
int main(void) { return 0; }
