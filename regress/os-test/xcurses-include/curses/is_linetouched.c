/*[SUSv2]*/
#include <curses.h>
#ifdef is_linetouched
#undef is_linetouched
#endif
bool (*foo)(WINDOW *, int) = is_linetouched;
int main(void) { return 0; }
