/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddwstr
#undef mvaddwstr
#endif
int (*foo)(int, int, const wchar_t *) = mvaddwstr;
int main(void) { return 0; }
