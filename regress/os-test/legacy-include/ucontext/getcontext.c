/*[SUSv3]*/
#include <ucontext.h>
#ifdef getcontext
#undef getcontext
#endif
int (*foo)(ucontext_t *) = getcontext;
int main(void) { return 0; }
