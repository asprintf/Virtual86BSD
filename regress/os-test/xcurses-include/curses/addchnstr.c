/*[SUSv2]*/
#include <curses.h>
#ifdef addchnstr
#undef addchnstr
#endif
int (*foo)(const chtype *, int) = addchnstr;
int main(void) { return 0; }
