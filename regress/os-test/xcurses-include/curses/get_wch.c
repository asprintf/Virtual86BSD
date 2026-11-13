/*[SUSv2]*/
#include <curses.h>
#ifdef get_wch
#undef get_wch
#endif
int (*foo)(wint_t *) = get_wch;
int main(void) { return 0; }
