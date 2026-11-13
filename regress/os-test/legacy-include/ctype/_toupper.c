/*[SUSv4]*/
#include <ctype.h>
#ifdef _toupper
#undef _toupper
#endif
int (*foo)(int) = _toupper;
int main(void) { return 0; }
