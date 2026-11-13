/*[SUSv2]*/
#include <curses.h>
#ifdef addnwstr
#undef addnwstr
#endif
int (*foo)(const wchar_t *, int) = addnwstr;
int main(void) { return 0; }
