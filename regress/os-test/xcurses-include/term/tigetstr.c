/*[XCurses7]*/
#include <term.h>
#ifdef tigetstr
#undef tigetstr
#endif
char *(*foo)(const char *) = tigetstr;
int main(void) { return 0; }
