/*[SUSv2]*/
#include <curses.h>
#ifdef erasewchar
#undef erasewchar
#endif
int (*foo)(wchar_t *) = erasewchar;
int main(void) { return 0; }
