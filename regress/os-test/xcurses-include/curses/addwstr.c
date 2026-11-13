/*[SUSv2]*/
#include <curses.h>
#ifdef addwstr
#undef addwstr
#endif
int (*foo)(const wchar_t *) = addwstr;
int main(void) { return 0; }
