/*[SUSv2]*/
#include <curses.h>
#ifdef typeahead
#undef typeahead
#endif
int (*foo)(int) = typeahead;
int main(void) { return 0; }
