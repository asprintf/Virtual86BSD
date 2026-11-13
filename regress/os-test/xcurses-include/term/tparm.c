/*[XCurses7]*/
#include <term.h>
#ifdef tparm
#undef tparm
#endif
char *(*foo)(const char *, long, long, long, long, long, long, long, long, long) = tparm;
int main(void) { return 0; }
