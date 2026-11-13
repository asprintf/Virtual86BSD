/*[SUSv2]*/
#include <curses.h>
#ifdef getbkgrnd
#undef getbkgrnd
#endif
int (*foo)(cchar_t *) = getbkgrnd;
int main(void) { return 0; }
