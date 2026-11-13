/*[XCurses7]*/
#include <term.h>
#ifdef tigetnum
#undef tigetnum
#endif
int (*foo)(const char *) = tigetnum;
int main(void) { return 0; }
