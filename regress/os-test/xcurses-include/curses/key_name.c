/*[SUSv2]*/
#include <curses.h>
#ifdef key_name
#undef key_name
#endif
char *(*foo)(wchar_t) = key_name;
int main(void) { return 0; }
