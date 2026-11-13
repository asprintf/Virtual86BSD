/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinwstr
#undef mvwinwstr
#endif
int (*foo)(WINDOW *, int, int, wchar_t *) = mvwinwstr;
int main(void) { return 0; }
