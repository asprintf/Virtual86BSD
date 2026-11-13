/*[SUSv2]*/
#include <curses.h>
#ifdef mvgetstr
#undef mvgetstr
#endif
int (*foo)(int, int, char *) = mvgetstr;
int main(void) { return 0; }
