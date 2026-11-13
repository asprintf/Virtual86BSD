/*[SUSv3]*/
#include <ucontext.h>
#ifdef makecontext
#undef makecontext
#endif
void (*foo)(ucontext_t *, void (*)(void), int, ...) = makecontext;
int main(void) { return 0; }
