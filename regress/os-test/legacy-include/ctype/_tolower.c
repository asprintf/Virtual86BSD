/*[SUSv4]*/
#include <ctype.h>
#ifdef _tolower
#undef _tolower
#endif
int (*foo)(int) = _tolower;
int main(void) { return 0; }
