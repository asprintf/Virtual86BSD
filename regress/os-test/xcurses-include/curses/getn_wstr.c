/*[SUSv2]*/
#include <curses.h>
#ifdef getn_wstr
#undef getn_wstr
#endif
int (*foo)(wint_t *, int) = getn_wstr;
int main(void) { return 0; }
