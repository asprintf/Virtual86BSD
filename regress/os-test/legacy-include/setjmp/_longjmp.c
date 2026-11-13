/*[SUSv4]*/
#include <setjmp.h>
#ifdef _longjmp
#undef _longjmp
#endif
void (*foo)(jmp_buf, int) = _longjmp;
int main(void) { return 0; }
