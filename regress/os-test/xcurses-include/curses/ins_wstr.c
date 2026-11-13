/*[SUSv2]*/
#include <curses.h>
#ifdef ins_wstr
#undef ins_wstr
#endif
int (*foo)(const wchar_t *) = ins_wstr;
int main(void) { return 0; }
