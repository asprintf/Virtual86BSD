/*[SUSv2]*/
#include <curses.h>
#ifdef ins_wch
#undef ins_wch
#endif
int (*foo)(const cchar_t *) = ins_wch;
int main(void) { return 0; }
