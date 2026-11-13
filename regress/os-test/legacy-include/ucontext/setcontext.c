/*[SUSv3]*/
#include <ucontext.h>
#ifdef setcontext
#undef setcontext
#endif
int (*foo)(const ucontext_t *) = setcontext;
int main(void) { return 0; }
