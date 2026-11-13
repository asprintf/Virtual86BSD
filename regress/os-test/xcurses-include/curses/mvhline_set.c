/*[SUSv2]*/
#include <curses.h>
#ifdef mvhline_set
#undef mvhline_set
#endif
int (*foo)(int, int, const cchar_t *, int) = mvhline_set;
int main(void) { return 0; }
