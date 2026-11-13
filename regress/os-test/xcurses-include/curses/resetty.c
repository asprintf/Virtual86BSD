/*[SUSv2]*/
#include <curses.h>
#ifdef resetty
#undef resetty
#endif
int (*foo)(void) = resetty;
int main(void) { return 0; }
