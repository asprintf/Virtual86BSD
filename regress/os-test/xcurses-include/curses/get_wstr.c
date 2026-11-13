/*[SUSv2]*/
#include <curses.h>
#ifdef get_wstr
#undef get_wstr
#endif
int (*foo)(wint_t *) = get_wstr;
int main(void) { return 0; }
