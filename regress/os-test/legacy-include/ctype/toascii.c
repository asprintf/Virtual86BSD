/*[SUSv4]*/
#include <ctype.h>
#ifdef toascii
#undef toascii
#endif
int (*foo)(int) = toascii;
int main(void) { return 0; }
