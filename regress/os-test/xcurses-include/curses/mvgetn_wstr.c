/*[SUSv2]*/
#include <curses.h>
#ifdef mvgetn_wstr
#undef mvgetn_wstr
#endif
int (*foo)(int, int, wint_t *, int) = mvgetn_wstr;
int main(void) { return 0; }
