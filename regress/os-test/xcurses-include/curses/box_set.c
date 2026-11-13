/*[SUSv2]*/
#include <curses.h>
#ifdef box_set
#undef box_set
#endif
int (*foo)(WINDOW *, const cchar_t *, const cchar_t *) = box_set;
int main(void) { return 0; }
