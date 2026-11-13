/*[SUSv2]*/
#include <curses.h>
#ifdef is_wintouched
#undef is_wintouched
#endif
bool (*foo)(WINDOW *) = is_wintouched;
int main(void) { return 0; }
