/*[SUSv3]*/
#include <ucontext.h>
#ifdef swapcontext
#undef swapcontext
#endif
int (*foo)(ucontext_t *, const ucontext_t *) = swapcontext;
int main(void) { return 0; }
