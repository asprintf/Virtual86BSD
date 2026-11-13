/*[SUSv2]*/
#include <curses.h>
#ifdef ins_nwstr
#undef ins_nwstr
#endif
int (*foo)(const wchar_t *, int) = ins_nwstr;
int main(void) { return 0; }
