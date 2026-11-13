/*[XCurses7]*/
#include <term.h>
#ifdef tigetflag
#undef tigetflag
#endif
int (*foo)(const char *) = tigetflag;
int main(void) { return 0; }
