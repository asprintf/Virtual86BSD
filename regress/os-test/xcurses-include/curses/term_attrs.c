/*[SUSv2]*/
#include <curses.h>
#ifdef term_attrs
#undef term_attrs
#endif
attr_t (*foo)(void) = term_attrs;
int main(void) { return 0; }
