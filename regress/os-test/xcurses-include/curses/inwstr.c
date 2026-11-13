/*[SUSv2]*/
#include <curses.h>
#ifdef inwstr
#undef inwstr
#endif
int (*foo)(wchar_t *) = inwstr;
int main(void) { return 0; }
