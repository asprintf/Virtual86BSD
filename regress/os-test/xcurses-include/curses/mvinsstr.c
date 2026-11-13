/*[SUSv2]*/
#include <curses.h>
#ifdef mvinsstr
#undef mvinsstr
#endif
int (*foo)(int, int, const char *) = mvinsstr;
int main(void) { return 0; }
