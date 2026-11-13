/*[SUSv2]*/
#include <curses.h>
#ifdef border_set
#undef border_set
#endif
int (*foo)(const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *) = border_set;
int main(void) { return 0; }
