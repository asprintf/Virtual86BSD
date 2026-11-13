/*[SUSv2]*/
#include <curses.h>
#ifdef pecho_wchar
#undef pecho_wchar
#endif
int (*foo)(WINDOW *, const cchar_t *) = pecho_wchar;
int main(void) { return 0; }
