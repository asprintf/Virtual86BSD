/*[SUSv4]*/
#include <setjmp.h>
#ifdef _setjmp
#undef _setjmp
#endif
int (*foo)(jmp_buf) = _setjmp;
int main(void) { return 0; }
