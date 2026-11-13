/*[SUSv2]*/
#include <curses.h>
#ifdef mvinwstr
#undef mvinwstr
#endif
int (*foo)(int, int, wchar_t *) = mvinwstr;
int main(void) { return 0; }
