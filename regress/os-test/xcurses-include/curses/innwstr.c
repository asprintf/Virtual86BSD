/*[SUSv2]*/
#include <curses.h>
#ifdef innwstr
#undef innwstr
#endif
int (*foo)(wchar_t *, int) = innwstr;
int main(void) { return 0; }
