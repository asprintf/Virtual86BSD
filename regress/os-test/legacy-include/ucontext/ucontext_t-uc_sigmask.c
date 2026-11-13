/*[SUSv3]*/
#include <ucontext.h>
void foo(ucontext_t* bar)
{
	sigset_t *qux = &bar->uc_sigmask;
	(void) qux;
}
int main(void) { return 0; }
