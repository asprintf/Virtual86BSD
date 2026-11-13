/*[SUSv4]*/
#include <ctype.h>
#ifdef isascii
#undef isascii
#endif
int (*foo)(int) = isascii;
int main(void) { return 0; }
