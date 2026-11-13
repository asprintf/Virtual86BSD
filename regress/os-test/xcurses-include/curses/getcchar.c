/*[SUSv2]*/
#include <curses.h>
#ifdef getcchar
#undef getcchar
#endif
int (*foo)(const cchar_t *, wchar_t *, attr_t *, short *, void *) = getcchar;
int main(void) { return 0; }
