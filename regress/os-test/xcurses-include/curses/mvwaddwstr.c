/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddwstr
#undef mvwaddwstr
#endif
int (*foo)(WINDOW *, int, int, const wchar_t *) = mvwaddwstr;
int main(void) { return 0; }
