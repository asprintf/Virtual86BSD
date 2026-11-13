/*[SUSv2]*/
#include <curses.h>
#ifdef bkgrnd
#undef bkgrnd
#endif
int (*foo)(const cchar_t *) = bkgrnd;
int main(void) { return 0; }
