/*[SUSv2]*/
#include <curses.h>
#ifdef unget_wch
#undef unget_wch
#endif
int (*foo)(const wchar_t) = unget_wch;
int main(void) { return 0; }
